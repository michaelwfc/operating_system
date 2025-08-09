#include <stdio.h>
#include <stdlib.h>
#include <wait.h>

// #include "kernel/types.h"
// #include "user/user.h"

// fork.c: create a new process

// The compiler cannot locate the header files kernel/types.h and user/user.h,
//  which are specific to an operating system (likely xv6) and not part of the standard C library. This causes include errors.



/**
 * stdlib.h
Full name: Standard Library header
Purpose: Part of the C standard library
Content: Contains declarations for general utility functions
Common functions:
Memory management: malloc(), calloc(), free()
Process control: exit(), atexit()
String conversion: atoi(), atof(), strtol()
Random number generation: rand(), srand()
Environment functions: getenv(), system()
Usage: Provides basic utility functions needed in most C programs

 * 
 */

void main()
{
    int pid = fork();
    if (pid > 0)
    {
        printf("parent : child=%d\n", pid);
        pid = wait((int *)0); // After the child exits, the parent’s wait returns
        printf("child=%d is done\n", pid);
    }
    else if (pid == 0)
    {
        printf("child : exiting\n");
        exit(0);
    }
    else
    {
        printf("fork error\n");
    }
}
