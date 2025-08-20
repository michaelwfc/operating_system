/**
 * $ trace 32 grep hello README
 * 
 */

#include "kernel/types.h"
#include "user/user.h"


void main(int argc, char *argv[]){
    int mask = atoi(argv[1]);
    if(trace(mask) <0){      // user wrapper -> syscall
        fprintf(2, "trace errord\n");
        exit(1);
    }
    exec(argv[2], argv+2);  // exec("grep", ["grep","hello","README"])
    exit(0);
}