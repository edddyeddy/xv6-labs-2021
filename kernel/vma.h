#ifndef VMA_H_
#define VMA_H_

struct VMA
{
    uint valid;
    uint64 fileStart;
    uint64 startAddress;
    uint64 length;
    int perm;
    int flag;
    struct file* file;

};

#endif
