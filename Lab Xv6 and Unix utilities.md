# Lab: Xv6 and Unix utilities

## Boot xv6 (easy)

只需要根据实验环境配置文档配置好XV6实验环境即可。

实验环境配置完成后在XV6目录下make qemu即可在qemu中运行xv6

```
➜  xv6-labs-2021 git:(util) make qemu
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2226
xargstest.sh   2 3 93
cat            2 4 23968
echo           2 5 22800
forktest       2 6 13176
grep           2 7 27328
init           2 8 23904
kill           2 9 22776
ln             2 10 22728
ls             2 11 26208
mkdir          2 12 22880
rm             2 13 22864
sh             2 14 41752
stressfs       2 15 23872
usertests      2 16 156088
grind          2 17 38048
wc             2 18 25112
zombie         2 19 22272
console        3 20 0
$
```

有趣的是XV6并没有ps命令，但可以通过Ctrl-p来打印正在运行的进程。

```
$
1 sleep  init
2 sleep  sh
```

若要退出的话按下Ctrl-a x即可。

## sleep (easy)

该部分需要实现用户程序sleep，其功能是根据用户传入的时间进行休眠。

在实现上只需要检查并转换传入的参数再调用sleep系统调用即可，代码如下

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
      printf("sleep error\n");
      exit(1);
  } 

  int time = atoi(argv[1]);
  sleep(time);

  exit(0);
}
```

通过测试

```
➜  xv6-labs-2021 git:(util) ✗ ./grade-lab-util sleep
make: 'kernel/kernel' is up to date.
== Test sleep, no arguments == sleep, no arguments: OK (2.3s) 
== Test sleep, returns == sleep, returns: OK (2.0s) 
== Test sleep, makes syscall == sleep, makes syscall: OK (2.0s) 
```

## pingpong (easy)

在该部分当中需要通过两个管道来实现父进程和子进程之间的双工通信，实现从父进程向子进程发送数据，子进程再将收到的数据发送回父进程的过程。

因此首先创建两个管道，分别用于父进程到子进程和子进程到父进程之间的消息传递。通过fork创建子进程，父进程和子进程只需要关闭不需要用到的管道描述符并且从管道中读取以及发送数据即可。

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define BUFFERSIZE 128

int main(int argc, char *argv[])
{
    int pipeOne[2]; // parent to child
    int pipeTwo[2]; // child to parent
    pipe(pipeOne);
    pipe(pipeTwo);

    int pid = fork();
    if (pid < 0)
    {
        printf("fork error\n");
    }
    else if (pid == 0)
    {
        // child process
        char buf[BUFFERSIZE];
        close(pipeOne[1]);
        close(pipeTwo[0]);
        read(pipeOne[0], buf, BUFFERSIZE);
        printf("%d: received ping\n", getpid());
        write(pipeTwo[1], buf, BUFFERSIZE);
    }
    else
    {
        // parent process
        char buf[BUFFERSIZE];
        close(pipeOne[0]);
        close(pipeTwo[1]);
        write(pipeOne[1], "a", 1);
        read(pipeTwo[0], buf, BUFFERSIZE);
        printf("%d: received pong\n", getpid());
    }

    exit(0);
}
```

测试通过

```
➜  xv6-labs-2021 git:(util) ✗ ./grade-lab-util pingpong
make: 'kernel/kernel' is up to date.
== Test pingpong == pingpong: OK (2.4s) 
```

## primes (moderate)/(hard)

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[])
{

    int leftPipe[2], rightPipe[2];
    pipe(leftPipe);

    // write number to first process
    for (int i = 2; i <= 35; i++)
    {
        write(leftPipe[1], &i, sizeof(i));
    }
    close(leftPipe[1]);

    for (;;)
    {
        int pid = fork();

        if (pid < 0)
        {
            fprintf(2, "fork error\n");
            exit(1);
        }
        else if (pid > 0)
        {
            wait(0);
            break;
        }
        else
        {
            // child process
            int num = 0, firstNum = 0;
            int ret = read(leftPipe[0], &firstNum, sizeof(num));
            // if no input , it is the final stage , exit
            if (ret == 0)
                exit(0);

            printf("prime %d\n", firstNum);

            pipe(rightPipe);

            while (read(leftPipe[0], &num, sizeof(num)))
            {
                if (num % firstNum)
                {
                    write(rightPipe[1], &num, sizeof(num));
                }
            }
            close(leftPipe[0]);
            close(rightPipe[1]);

            // swap left and right pipe
            for(int i = 0 ; i < 2 ; i++){
                int t = leftPipe[i];
                leftPipe[i] = rightPipe[i];
                rightPipe[i] = t;
            }
        }
    }

    exit(0);
}
```

测试通过

```
➜  xv6-labs-2021 git:(util) ✗ ./grade-lab-util primes
make: 'kernel/kernel' is up to date.
== Test primes == primes: OK (3.6s) 
```

## find (moderate)

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define BUFSIZE 128

char *getFileName(char *path)
{
    char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    return p;
}

int find(char *path, char *target)
{
    char buf[BUFSIZE], *p;
    int fd;
    struct dirent de;
    struct stat st;
    // printf("path = %s\n", path);

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "open file error\n");
        return 1;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "get file stat error\n");
        return 1;
    }

    switch (st.type)
    {
    case T_FILE:
        if (strcmp(getFileName(path), target) == 0)
        {
            printf("%s\n", path);
        }
        break;

    case T_DIR:
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            {
                continue;
            }
            memset(buf, '\0', BUFSIZE);
            strcpy(buf, path);
            p = buf + strlen(buf);
            *(p++) = '/';
            strcpy(p, de.name);
            find(buf, target);
        }
        break;
    }

    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("usage: find dirent filename\n");
        exit(1);
    }

    int ret = find(argv[1], argv[2]);

    exit(ret);
}

```

测试通过

```
➜  xv6-labs-2021 git:(util) ✗ ./grade-lab-util find
make: 'kernel/kernel' is up to date.
== Test find, in current directory == find, in current directory: OK (4.5s) 
== Test find, recursive == find, recursive: OK (4.2s) 
```

## xargs (moderate)

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define BUFSIZE 128

int copyOriginArgv(char *dest[], char *src[])
{
    int d = 0;
    for (int s = 1; src[s]; s++, d++)
    {
        dest[d] = src[s];
    }
    return d;
}

void execNewProc(char *path, char **newArgv)
{
    // exec new process
    int pid = fork();
    if (pid < 0)
    {
        fprintf(2, "fork error\n");
    }
    else if (pid == 0)
    {
        exec(newArgv[0], newArgv);
    }
    else
    {
        wait(0);
    }
}

void catNewArgv(int startIndex, char *newArgv[], char *command)
{
    char *p = command;
    while (*p != '\0')
    {
        newArgv[startIndex++] = p;
        while (*p != ' ' && *p != '\0')
        {
            p++;
        }
        if (*p == ' ')
        {
            *p = '\0';
            while (*p != '\0' && *p == ' ')
            {
                p++;
            }
        }
    }
    newArgv[startIndex++] = '\0';
}

void execOneLine(int startIndex, char *newArgv[], char *commandLine)
{
    catNewArgv(startIndex, newArgv, commandLine);
    execNewProc(newArgv[0], newArgv);
}

int main(int argc, char *argv[])
{
    char *newArgv[MAXARG], buf[BUFSIZE];
    int index = copyOriginArgv(newArgv, argv);

    while (gets(buf, BUFSIZE))
    {
        if (*buf == '\0')
            break;

        // del the '\n'
        buf[strlen(buf) - 1] = '\0';

        execOneLine(index, newArgv, buf);
    }

    exit(0);
}
```

测试通过

```
➜  xv6-labs-2021 git:(util) ✗ ./grade-lab-util xargs
make: 'kernel/kernel' is up to date.
== Test xargs == xargs: OK (4.4s) 
```

## make grade

```
== Test sleep, no arguments == 
$ make qemu-gdb
sleep, no arguments: OK (6.7s) 
== Test sleep, returns == 
$ make qemu-gdb
sleep, returns: OK (2.3s) 
== Test sleep, makes syscall == 
$ make qemu-gdb
sleep, makes syscall: OK (2.1s) 
== Test pingpong == 
$ make qemu-gdb
pingpong: OK (2.3s) 
== Test primes == 
$ make qemu-gdb
primes: OK (2.5s) 
== Test find, in current directory == 
$ make qemu-gdb
find, in current directory: OK (2.6s) 
== Test find, recursive == 
$ make qemu-gdb
find, recursive: OK (3.7s) 
== Test xargs == 
$ make qemu-gdb
xargs: OK (4.7s) 
== Test time == 
time: OK 
Score: 100/100
```

