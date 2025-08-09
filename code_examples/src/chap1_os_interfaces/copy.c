#include "kernel/types.h"
#include "user/user.h"

// #include <unistd.h>
// #include <stdlib.h> //for exit()

int main(){
    char buf[10];
    while(1){
        int n = read(0, buf, sizeof(buf));
        if(n<=0) break;
        write(1, buf, n);
    }
    exit(0);
}