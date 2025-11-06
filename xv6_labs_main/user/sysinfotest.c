// #include "kernel/types.h"
#include "kernel/sysinfo.h"  //build the struct of sysinfo
#include "user/user.h"      // decleare the system call function of sysinfo

void sysinfotest(void){
    struct sysinfo si;
    if(sysinfo(&si)<0){
        fprintf(2, "sysinfo error\n");
        exit(1);
    }
    printf("free memory: %d bytes, num of process: %d\n",si.freemem,si.nproc);
    exit(0);
}