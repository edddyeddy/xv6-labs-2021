// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
  struct spinlock bucketLock[NBUCKET];
} bcache;

int blockHash(uint dev, uint blockno){
  return (dev + blockno) % NBUCKET;
}

struct buf* 
getEmptyBlockInBucket(int hash, uint64 *minTime , struct buf **LRUBuf){
  struct buf * b;
  for(b = bcache.head[hash].next ; b != &bcache.head[hash] ; b = b->next){
    if(b->refcnt == 0){
      b->valid = 0;
      b->refcnt = 1;
      // extract block from list
      b->prev->next = b->next;
      b->next->prev = b->prev;
      acquiresleep(&b->lock);
      return b;
    }
    if(minTime && b->timeStamp < *minTime){
      *minTime = b->timeStamp;
      *LRUBuf = b;
    }
  }
  return 0;
}

void
binit(void)
{
  struct buf *b;

  // init the global hash table lock and bucket lock
  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKET; i++){
    bcache.head[i].next = &bcache.head[i];
    bcache.head[i].prev = &bcache.head[i];
    initlock(&bcache.bucketLock[i],"bcache.bucket");
  }
  // put all buffers in bucket 0
  struct buf *head = &bcache.head[0];
  for(int i = 0 ; i < NBUF ; i++){
    b = &bcache.buf[i];
    initsleeplock(&b->lock,"buffer");
    b->next = head->next;
    b->prev = head;
    head->next->prev = b;
    head->next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // Is the block already cached?
  int hash = blockHash(dev,blockno);
  acquire(&bcache.bucketLock[hash]);  // bucket lock

  for(b = bcache.head[hash].next; b != &bcache.head[hash] ; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucketLock[hash]);
      acquiresleep(&b->lock);
      b->timeStamp = ticks;
      return b;
    }
  }

  // Not cached.
  // search the empty buffer in the same bucket
  b = getEmptyBlockInBucket(hash,0,0);
  if(b){
    b->dev = dev;
    b->blockno = blockno;
    b->timeStamp = ticks;
    // insert the block to list
    b->next = bcache.head[hash].next;
    b->prev = &bcache.head[hash];
    bcache.head[hash].next->prev = b; 
    bcache.head[hash].next = b;
    release(&bcache.bucketLock[hash]);
    return b;
  }
  release(&bcache.bucketLock[hash]);

  // search the empty buffer in all bucket
  uint64 minTime = 0xFFFFFFFF;
  struct buf *LRUBuf = 0;
  for (int i = 0; i < NBUF; i++)
  {
    acquire(&bcache.bucketLock[i]);
    b = getEmptyBlockInBucket(i,&minTime,&LRUBuf);
    if(b){
      b->dev = dev;
      b->blockno = blockno;
      b->timeStamp = ticks;
      // insert the block to list
      b->next = bcache.head[hash].next;
      b->prev = &bcache.head[hash];
      bcache.head[hash].next->prev = b; 
      bcache.head[hash].next = b;
      // release block
      release(&bcache.bucketLock[i]);
      return b;
    }
    release(&bcache.bucketLock[i]);
  }

  // not find empty block
  // select the LRU block
  // global lock
  acquire(&bcache.lock);

  int lruHash = blockHash(LRUBuf->dev,LRUBuf->blockno);
  acquire(&bcache.bucketLock[lruHash]);
  LRUBuf->prev->next = LRUBuf->next;
  LRUBuf->next->prev = LRUBuf->prev;
  release(&bcache.bucketLock[lruHash]);

  LRUBuf->dev = dev;
  LRUBuf->blockno = blockno;
  LRUBuf->refcnt = 1;
  LRUBuf->valid = 0;

  // insert the block to list
  acquire(&bcache.bucketLock[hash]);
  b->next = bcache.head[hash].next;
  b->prev = &bcache.head[hash];
  bcache.head[hash].next->prev = b; 
  bcache.head[hash].next = b;
  release(&bcache.bucketLock[hash]);
  
  release(&bcache.lock);

  acquiresleep(&b->lock);
  b->timeStamp = ticks;
  return b;

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  int hash = blockHash(b->dev , b->blockno);
  releasesleep(&b->lock);

  acquire(&bcache.bucketLock[hash]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->valid = 0;
  }
  release(&bcache.bucketLock[hash]);
}

void
bpin(struct buf *b) {
  int hash = blockHash(b->dev,b->blockno);
  acquire(&bcache.bucketLock[hash]);
  b->refcnt++;
  release(&bcache.bucketLock[hash]);
}

void
bunpin(struct buf *b) {
  int hash = blockHash(b->dev,b->blockno);
  acquire(&bcache.bucketLock[hash]);
  b->refcnt--;
  release(&bcache.bucketLock[hash]);
}


