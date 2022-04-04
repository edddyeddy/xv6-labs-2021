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
