#ifndef __PTHREAD_H__
#define __PTHREAD_H__

#include <stdint.h>
typedef uint64_t pthread_t;
typedef uint64_t pthread_attr_t;
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
void pthread_yield(void);
void pthread_exit(void *retval);
#endif