#include <atomic.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define TOLERANCE 2

atomic_t yield_count1 = ATOMIC_INIT(0);
atomic_t yield_count2 = ATOMIC_INIT(0);
int line1 = 18;
int line2 = 19;
atomic_t var1 = ATOMIC_INIT(0);
atomic_t var2 = ATOMIC_INIT(0);
void *thread1_function(void *arg) {
  atomic_t *ptr_var1 = (atomic_t *)arg;
  while (1) {
    atomic_inc(ptr_var1);
    sys_move_cursor(0, line1);
    printf("Thread 1: var1 = %d\n", atomic_read(ptr_var1));
    if (atomic_read(ptr_var1) - atomic_read(&var2) >= TOLERANCE) {
      atomic_inc(&yield_count1);
      pthread_yield();
    }
  }
  // pthread_exit(NULL);
}

void *thread2_function(void *arg) {
  atomic_t *ptr_var2 = (atomic_t *)arg;
  while (1) {
    atomic_inc(ptr_var2);
    sys_move_cursor(0, line2);
    printf("Thread 2: var2 = %d\n", atomic_read(ptr_var2));
    if (atomic_read(ptr_var2) - atomic_read(&var1) >= TOLERANCE) {
      atomic_inc(&yield_count2);
      pthread_yield();
    }
  }
  // pthread_exit(NULL);
}

int main() {
  pthread_t thread1 = 0, thread2 = 0;
  if (pthread_create(&thread1, NULL, thread1_function, &var1) != 0) {
    // TODO: perror?
  }

  if (pthread_create(&thread2, NULL, thread2_function, &var2) != 0) {
    // TODO: perror?
  }
  while (1) {
    sys_move_cursor(0, line1 + 2);
    printf("Thread 1 yielded %ld times, is scheduled %d times\n", atomic_read(&yield_count1), sys_get_sched_times(thread1));
    sys_move_cursor(0, line2 + 2);
    printf("Thread 2 yielded %ld times, is scheduled %d times\n", atomic_read(&yield_count2), sys_get_sched_times(thread2));
  }

  return 0;
}
