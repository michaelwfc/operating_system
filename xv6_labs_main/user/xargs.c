/**
 * Write a simple version of the UNIX xargs program: read lines from the standard input and run a command for each line, supplying the line as arguments to the command. 
 * Your solution should be in the file user/xargs.c.
 * 
 */
#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"

 int main(int argc, char *argv[])
 {
     if(argc < 2){
         fprintf(2, "Usage: xargs <command> [initial-args]\n");
     }
     
     char buf[512];
     char *newargv[MAXARG];

     // copy  the original arguments into newargv
     for(int i = 1; i < argc; i++){
        newargv[i-1] = argv[i];
     }

     // read from stdin util EOF
     while(1){
        int n=0;
        char c;
        // read one argument (delimited by space or newline)
        // 你现在的 while((read(0, &c, 1))==1 && c != '\n' && c!='\0') 只能处理一行一个参数， 如果一行有多个参数（以空格分隔），会被当成一个整体。
        while((read(0, &c, 1))==1 && c != '\n' && c!='\0'){
            buf[n++] = c;
        }
        if(n==0){
            break;
        }
        buf[n] = '\0'; // add null-terminate for string
        // 你直接用 newargv[argc-1] 填参数，这假设了每次只增加一个新参数， 如果有多个输入单词，就会覆盖错误位置。
        newargv[argc-1] = buf;
        newargv[argc] = 0; // null terminate for exec

        if(fork()==0){
            // printf("exec %s\n", argv[1]);
            exec(argv[1], newargv);
            fprintf(2, "xargs: exec %s failed\n", argv[1]);
            exit(1);
        }
        wait(0);
        if(c==0) break;
     }
     exit(0);
 }