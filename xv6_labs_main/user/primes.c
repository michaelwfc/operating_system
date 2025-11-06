/**Your goal is to use pipe and fork to set up the pipeline. 
 * The first process feeds the numbers 2 through 35 into the pipeline. 
 * For each prime number, you will arrange to create one process that reads from its left neighbor over a pipe and writes to its right neighbor over another pipe. 
 * Since xv6 has limited number of file descriptors and processes, the first process can stop at 35.
 * 
1. main process:
- create the first pipe pipe1
- writes 2..35 into pipe1.
- Closes the write end and forks the first prime filter process.

2. Prime filter process
- Reads the first number from the pipe → this is the prime.
- Prints it.
- Creates a new pipe for the next process.
- Filters out numbers divisible by this prime and writes the rest into the new pipe.
- Forks the next prime filter process and passes the new pipe to it.
- This repeats until no more numbers remain.
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void filter_primes(int pleft[2]) {
    // the input is from the left pipe
    close(pleft[1]); // close the write end of left/input pipe

    int prime;
    if(read(pleft[0], &prime, sizeof(int))==0){
        //  read returns zero when the write-side of a pipe is closed
        // no more numbers, close the pleft and exit 
        close(pleft[0]); 
        exit(0);
    }
    printf("prime %d\n", prime); // get the first number in the pipe

    int pright[2];
    pipe(pright); //create a new pipe right
    int pid = fork();
    if(pid==0){
        // child: receive numbers from pright
        close(pleft[0]);    // in child close the read end of left pipe
        filter_primes(pright); // filter iterative
    }else{
        // parent: filter out the multiples of prime
        close(pright[0]); // close the read end of the right pipe
        int number;
        while(read(pleft[0], &number, sizeof(int))>0){
            if(number%prime!=0){
                write(pright[1], &number, sizeof(int)); // pase filtered numbers to the right pipe
            }
        }
        close(pleft[0]); // after filter all the numbers, close the read end of the input pipe
        close(pright[1]); // close the write end of the output pipe
        wait(0);
        exit(0);
    }

    }


 int main(void) { 
    int p[2];
    pipe(p);

    int pid = fork();
    if(pid==0){
       
        filter_primes(p);
    }else{
        close(p[0]); // parent closes read end
        for(int i=2; i<=35; i++){
            write(p[1], &i, sizeof(int)); // write to pipe write end to pass the number
        }
        // close the write end and send EOF signal,
        // read returns zero when the write-side of a pipe is closed
        close(p[1]); 
        wait(0);
    }
    exit(0);
 }