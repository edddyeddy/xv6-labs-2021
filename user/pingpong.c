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
