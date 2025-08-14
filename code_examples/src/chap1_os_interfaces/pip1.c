
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
    // Both ends are inherited across fork(). Both parent and child have copies of p[0] and p[1].
    // File descriptors refer to the same underlying pipe.

    // both ends of the pipe exist in every process created after this call (via fork()), unless you explicitly close() the ends you don't use.
    // Think of a pipe as having two key behaviors:
    // - Reading from a pipe blocks until there is data or all write ends are closed.
    // - Writing to a pipe blocks if the pipe buffer is full, until some process reads from it.
    if (pid == 0)
    {
        // The child calls close  stdin
        close(0);
        // duplicates p[0]  the read end of the pipe into the lowest available fd (which is now 0).
        // stdin now refers to the pipe’s read end.
        dup(p[0]);

        //These are closed to clean up; the child will read only via its stdin (fd 0).
        // Closing unused ends is critical — prevents deadlocks.

        close(p[0]);
        
        // If you forget to close(p[1]) in the child: 
        // The read in the child never sees EOF, because the child itself still has a copy of the write end open.
        // Even if the parent closes its write end, the kernel thinks “someone (child) still can write,” so it doesn’t send EOF → the read() hangs forever.
        close(p[1]);
        // Rule of thumb:
        // Writer process: close all unused read ends.
        // Reader process: close all unused write ends.
        // This ensures the kernel can send EOF correctly and avoid blocking forever.
         
        //If the write end (p[1]) stayed open in the child, wc would wait forever (since the pipe wouldn't signal EOF).
        // Replaces the child’s process image with wc.
        // Since stdin is now the pipe’s read end, wc reads data sent by the parent.
        // When wc reads from its standard input, it reads from the pipe. calls exec to run wc
        execvp("/bin/wc", argv);
    }
    else
    {
        // The parent closes the read side of the pipe, 
        // Parent won’t read from the pipe — closes read end immediately.

        // If you forget to close(p[0]) in the parent:
        // If the pipe buffer fills up, the parent’s write() may block waiting for the child to read.
        // But the child might also be waiting for input from the parent (and not reading yet), so you get a deadlock.
        close(p[0]);
        // writes to the pipe
        write(p[1], "hello world\n", 12);
        // Closing the write end sends EOF to the child, so wc stops reading and prints results.
        close(p[1]);
    }
}
