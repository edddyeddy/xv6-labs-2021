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
