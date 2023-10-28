#include <kernel.h>
#include <stdint.h>
#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include <pthread.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4) 
{
  /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
  // use inline asm to call system call
  // first put your arguments and sysno into registers
  // then trigger the system call by `ecall`
  // finally get the return value from registers
  long ret;
  asm volatile("mv a0, %1\n\t"
               "mv a1, %2\n\t"
               "mv a2, %3\n\t"
               "mv a3, %4\n\t"
               "mv a4, %5\n\t"
               "mv a7, %6\n\t"
               "ecall\n\t"
               "mv %0, a0\n\t"
               : "=r"(ret)
               : "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4),
                 "r"(sysno)
               : "a0", "a1", "a2", "a3", "a4", "a7");
  return ret;
}

void sys_yield(void) 
{
  invoke_syscall(SYSCALL_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor(int x, int y) 
{
  invoke_syscall(SYSCALL_CURSOR, x, y, IGNORE, IGNORE, IGNORE);
}

void sys_write(char *buff) 
{
  invoke_syscall(SYSCALL_WRITE, (long)buff, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_reflush(void) 
{
  invoke_syscall(SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mutex_init(int key) 
{
  return invoke_syscall(SYSCALL_LOCK_INIT, key, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_acquire(int mutex_idx) 
{
  invoke_syscall(SYSCALL_LOCK_ACQ, mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_release(int mutex_idx) 
{
  invoke_syscall(SYSCALL_LOCK_RELEASE, mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

long sys_get_timebase(void) 
{
  return invoke_syscall(SYSCALL_GET_TIMEBASE, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

long sys_get_tick(void) {
  return invoke_syscall(SYSCALL_GET_TICK, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_sleep(uint32_t time) {
  invoke_syscall(SYSCALL_SLEEP, time, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_strace(uint64_t strace_bitmask) {
  invoke_syscall(SYSCALL_STRACE, strace_bitmask, IGNORE, IGNORE, IGNORE, IGNORE);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) 
{
  return invoke_syscall(SYSCALL_THREAD_CREATE, (long)thread, (long)attr,
                        (long)start_routine, (long)arg, IGNORE);
}

void pthread_yield(void) 
{
  invoke_syscall(SYSCALL_THREAD_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void pthread_exit(void *retval)
{
  invoke_syscall(SYSCALL_THREAD_EXIT, (long)retval, IGNORE, IGNORE, IGNORE, IGNORE);
}
/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/