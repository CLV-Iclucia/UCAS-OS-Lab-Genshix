#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#define NUM_THREADS 3
// generated by chatGPT to test whether my implementation of threads is correct

void threadFunction(void *threadId) {
    long tid;
    tid = (long)threadId;
    sys_move_cursor(0, tid + 1);
    printf("Thread #%ld: Hello, World!\n", tid);
}

int main() {
    pthread_t threads[NUM_THREADS];
    int rc;
    long t;
    sys_move_cursor(0, NUM_THREADS + 1);
    for (t = 0; t < NUM_THREADS; t++) {
        printf("Main: Creating thread %ld\n", t);
        rc = pthread_create(&threads[t], NULL, threadFunction, (void *)t);
        if (rc != 0) {
            printf("Error: return code from pthread_create() is %d\n", rc);
            return 1;
        }
    }
    
    for (t = 0; t < NUM_THREADS; t++) {
        rc = pthread_join(threads[t]);
        printf("Main: Thread %ld returned %d\n", t, rc);
    }

    return 0;
}
