#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>     // Added for O_RDONLY
#include <sys/wait.h>  // Added for wait() if needed

int main(){
    char* argv[2];
    argv[0] = "cat";
    argv[1] = 0;     // NULL terminator - required
    int pid = fork();
    if(pid==0){
        // After the child closes file descriptor 0, open is guaranteed to use that file descriptor for the newly opened input.txt:
        //  0 will be the smallest available file descriptor.
        close(0);
        
        // Since file descriptor 0 is now available, open() assigns input.txt to file descriptor 0
        // This means stdin now points to input.txt instead of the terminal
        int fd= open("input.txt",O_RDONLY);
        
        if(fd<0){
            perror("Failed to open input.txt");
            exit(1);
        }else{
            printf("Opened input.txt  in child with fd= %d\n",fd);
        }
        // The argv array only contains the command name, not the file name
        // When cat is executed without arguments, it reads from stdin by default
        // Since you've redirected stdin to input.txt, cat reads from that file
        execv("/bin/cat",argv);
        perror("Failed to execute cat"); // Only reached if execv fails
        exit(1);

    }else if(pid>0){
        printf("Parent process with child pid: %d\n",pid);
        // without wait(), the parent process may terminate before the child process completes its execution and outputs the file content to the console.
        wait(0);
        printf("Parent process done\n");
    }else{
        printf("Error in fork\n");
    }
    exit(0);
}