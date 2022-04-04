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
