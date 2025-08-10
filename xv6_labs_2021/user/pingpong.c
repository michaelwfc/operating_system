#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/**
 * Write a program that uses UNIX system calls to ''ping-pong'' a byte between two processes over a pair of pipes, one for each direction. 
 * The parent should send a byte to the child; the child should print "<pid>: received ping", where <pid> is its process ID, write the byte on the pipe to the parent, and exit; 
 * the parent should read the byte from the child, print "<pid>: received pong", and exit. Your solution should be in the file user/pingpong.c.
 */

 int
main(int argc, char *argv[])
{
    int p1[2]; // parent to child
    int p2[2]; // child to parent
    pipe(p1);
    pipe(p2);

    int pid = fork(); 
    if(pid==0){
        close(p1[1]);
        // child process
        char byteBuffer[1];
        read(p1[0], byteBuffer, 1);
        printf("%d: received ping\n");
        // printf("%d: received ping from parent: %c\n", getpid(), byteBuffer[0]);
        close(p1[0]); 
        write(p2[1], "c", 1); // write a byte to pipe write end
        close(p2[1]);
        exit(0);
    }else if(pid>0){
        close(p2[1]);
        // parent process
        write(p1[1], "p", 1); // write a byte to pipe write end
        close(p1[1]); // close the pipe write end and send EOF signal
        // waited for the child to finish
        char byteBuffer[1];
        read(p2[0], byteBuffer, 1);
        printf("%d: received pong\n");
        // printf("%d: received pong from child: %c\n", getpid(), byteBuffer[0]);
        close(p2[0]);
        int status;
        wait(&status);
        // printf("%d: child exited with status %d\n", getpid(), status);
        exit(0);
    }else{
        printf("fork failed\n");
        exit(1);
    }
    exit(0);
}