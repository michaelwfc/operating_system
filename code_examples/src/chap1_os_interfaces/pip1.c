
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // For pipe(), fork(), dup(), close(), write(), execvp()

/**
 * The following example code runs the program wc with standard input connected to the read end of a pipe.
 *
 * This code demonstrates how to use pipes to connect processes in Unix-like systems.
 * It creates a pipe and uses it to send data from the parent process to the wc (word count) command running in a child process.
 * 
 * This code demonstrates a fundamental Unix concept: using pipes to connect the output of one process to the input of another, 
 * similar to shell pipelines like echo "hello world" | wc.
 * 
 */
int main()
{
    printf("This program demonstrates pipes.\n");
    int p[2];
    char *argv[2];
    argv[0] = "wc";
    argv[1] = NULL;

    // The program calls pipe, which creates a new pipe and records the read and write file descriptors in the array p.
    // p[0]: read end of the pipe
    // p[1]: write end of the pipe
    pipe(p);

    int pid = fork();
    // After fork, both parent and child have file descriptors referring to the pipe
    if (pid == 0)
    {
        // The child calls close and dup to make file descriptor zero refer to the read end of the pipe,
        close(0);
        // Uses dup(p[0]) to duplicate the read end of the pipe to file descriptor 0
        dup(p[0]);
        close(p[0]);
        // closes the file descriptors in p
        close(p[1]);
        // calls exec to run wc, When wc reads from its standard input, it reads from the pipe.
        execvp("/bin/wc", argv);
    }
    else
    {
        // The parent closes the read side of the pipe, 
        close(p[0]);
        // writes to the pipe, and then closes the write side
        write(p[1], "hello world\n", 12);
        close(p[1]);
    }
}
