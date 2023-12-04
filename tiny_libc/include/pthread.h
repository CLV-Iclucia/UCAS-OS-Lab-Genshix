#ifndef PTHREAD_H_
#define PTHREAD_H_
#include "unistd.h"

typedef int pthread_t;
typedef int pthread_attr_t;
/* TODO:[P4-task4] pthread_create/wait */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                    void (*start_routine)(void *), void *arg);

int pthread_join(pthread_t thread);

#endif