#include <stdio.h>
#include <unistd.h>

/**
 * unistd.h
Full name: Unix Standard header
Purpose: Part of the POSIX (Portable Operating System Interface) standard
Content: Contains declarations for system calls and other standard Unix functions
Common functions:
exec() family functions (execv, execl, etc.)
fork(), pipe(), dup()
sleep(), usleep()
File I/O operations like read(), write()
Process control functions
Usage: Essential for system-level programming on Unix-like systems (Linux, macOS, etc.)


// Note: exec() is not a single function, but a family of functions (execl, execv, execvp, etc.), all declared in <unistd.h>.

The unistd.h header provides several variants of the exec function:

execl() - Takes command as separate arguments
execlp() - Searches in PATH, takes command as separate arguments
execle() - Like execl() but also specifies environment
execv() - Takes command as an array of strings
execvp() - Like execv() but searches in PATH
execvpe() - Like execvp() but also specifies environment


 *
 */
void main()
{
    char *argv[3];
    argv[0] = "echo";        // Program name (convention)
    argv[1] = "hello world"; // First argument to the echo command
    argv[2] = 0;             // NULL terminator - required
    
    // If successful, execv() never returns (the current process is replaced)
    // If execv() fails, it returns -1 and the next line
    execv("/bin/echo", argv);
    printf("exec failed\n");
}