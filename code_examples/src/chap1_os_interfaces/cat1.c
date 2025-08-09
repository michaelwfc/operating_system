#include <stdio.h>  // for printf() and fprintf()
#include <unistd.h> // for read() and write() system calls
#include <stdlib.h> //for exit()

char buf[1024];
int n;
int main(void)
{

    for (;;)
    {
        n = read(0, buf, sizeof buf);
        if (n == 0)
            break;
        if (n < 0)
        {
            // fprintf(2, "read error\n");
            // This fix resolves the type mismatch warning by using the proper FILE* pointer instead of the raw file descriptor integer.
            fprintf(stderr, "read error\n");
            exit(1);
        }
        if (write(1, buf, n) != n)
        {
            // fprintf(2, "write error\n");
            fprintf(stderr, "write error\n");
            exit(1);
        }
    }
    exit(0);
}