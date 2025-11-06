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
        // Close unused ends
        close(p1[1]);  // child doesn't write to p1
        close(p2[0]);  // child doesn't read from p2

        // child process Read one byte from parent from p1 read end
        char byteBuffer[1];
        read(p1[0], byteBuffer, 1);
        printf("%d: received ping\n");
        // printf("%d: received ping from parent: %c\n", getpid(), byteBuffer[0]);
        close(p1[0]); 

        write(p2[1], "c", 1); // write a byte to pipe 2 write end
        close(p2[1]);         // close pipe 2 write end then send EOF single
        exit(0);
    }else if(pid>0){
        // close unused ends
        close(p1[0]); // parent doesn't need pipe 1 read end
        close(p2[1]); // parent doesn't need pipe 2 write end

        // parent process write a byte to pipe 1 write end
        write(p1[1], "p", 1); 
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