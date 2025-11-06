/**
 * Write a simple version of the UNIX find program: find all the files in a directory tree with a specific name. Your solution should be in the file user/find.c.
 * Some hints:
Look at user/ls.c to see how to read directories.
Use recursion to allow find to descend into sub-directories.
Don't recurse into "." and "..".
Changes to the file system persist across runs of qemu; to get a clean file system run make clean and then make qemu.
You'll need to use C strings. Have a look at K&R (the C book), for example Section 5.5.
Note that == does not compare strings like in Python. Use strcmp() instead.
Add the program to UPROGS in Makefile.
 *
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
void find(char *path, char *name)
{ // inum → the inode number (2 bytes)
    // name[DIRSIZ] → file name, always stored in a 14-byte fixed-length array (not null-terminated if full)
    // Direct indexing — directories in xv6 are just flat arrays of struct dirent written into a file. If each entry is fixed-size, you can easily compute:

    struct dirent de;
    struct stat st;
    char buf[512], *p;
    int fd;
    if ((fd = open(path, 0)) < 0)
    { // open the path in read-only mode
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        fprintf(2, "find: %s is a file\n", path);
        break;
    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf))
        {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;

            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            { // get the stat of the file
                continue;
            }
            // move the name to the end of buf
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0)
            { // get the stat of the file
                continue;
            }

            // check the type
            switch (st.type)
            {
            case T_FILE: // compare the filename
                if (strcmp(de.name, name) == 0)
                {
                    fprintf(1, "%s\n", buf); // compare the filename start from p with name
                }
                break;
            case T_DIR:
                find(buf, name); // recursive call
                break;
            }
        }
    }

    close(fd);
}
void main(int argc, char *argv[])
{
    if (argc == 1 || argc > 3)
    {
        printf("Usage: find path name\n");
        exit(0);
    }
    else if (argc == 2)
    {
        find(".", argv[1]);
        exit(0);
    }
    else
    {
        find(argv[1], argv[2]);
    }

    exit(0);
}