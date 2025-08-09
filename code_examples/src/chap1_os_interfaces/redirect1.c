#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>  //Add #include <fcntl.h> to access the file operation flags
#include <sys/wait.h> // Include for wait() function

int main(){
    int pid;
    pid = fork();
    if(pid == 0){
        // redirect stdout to output.txt
        close(1);
        open("output.txt", O_CREAT | O_WRONLY);
        char *args[] = {"echo", "hello world", NULL};
        execv("/bin/echo", args);
        printf("exec failed\n");
        exit(1);
    }else{
        wait((int *)0);

    }
    exit(0);
}