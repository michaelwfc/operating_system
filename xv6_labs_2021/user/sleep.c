
#include "kernel/types.h"
#include "kernel/stat.h"
// #include "kernel/syscall.h" //sysproc.c for implements the sleep system call (look for sys_sleep)
#include "user/user.h" // for atoi, for the C definition of sleep callable from a user program,


// In xv6, user programs should typically use int main() as the standard signature
int 
main(int argc, char *argv[]){
    if(argc != 2){
        fprintf(2, "Usage: sleep seconds\n");
        exit(1);
    }
    //The command-line argument is passed as a string; you can convert it to an integer using atoi (see user/ulib.c).
    //The user library function (sleep) in user/usys.S makes the actual system call to the kernel
    // Your code is just a user program using the existing system call interface
    sleep(atoi(argv[1]));
    exit(0);

}