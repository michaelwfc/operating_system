#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


#define NTHREADS 5

void *thread(void *vargp);

int main()
{ 
    pthread_t tid[NTHREADS];

    for(int i = 0; i < NTHREADS; i++){
        int *arg = malloc(sizeof(int));  
        *arg = i;
        int result = pthread_create(&tid[i], NULL, thread, arg); 
        if (result != 0) {
            printf("Thread creation failed: %d\n", result);
            free(arg);
        }
    }   
    // Wait for all threads to complete
    for(int i = 0; i < NTHREADS; i++){
        pthread_join(tid[i], NULL);
    }
    
    // exit(0);
    return 0;
}

void *thread(void *vargp)
{   
    int thread_id = *(int *)vargp;
    printf("Hello World from thread %d\n", thread_id);
    free(vargp);
    // pthread_exit(NULL);
    return NULL;
}
