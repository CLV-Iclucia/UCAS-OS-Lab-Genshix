#include <kernel.h>
#include <pthread.h>
#include <stdint.h>
#include <syscall.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4) {
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

  return 0;
}

void sys_yield(void) {
  invoke_syscall(SYSCALL_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor(int x, int y) {
  invoke_syscall(SYSCALL_CURSOR, x, y, IGNORE, IGNORE, IGNORE);
}

void sys_clear_region(int x, int y, int w, int h) {
  invoke_syscall(SYSCALL_CLEAR_REGION, x, y, w, h, IGNORE);
}

void sys_move_cursor_x(int x) {
  invoke_syscall(SYSCALL_CURSOR, x, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor_y(int y) {
  invoke_syscall(SYSCALL_CURSOR, IGNORE, y, IGNORE, IGNORE, IGNORE);
}

void sys_write(char *buff) {
  invoke_syscall(SYSCALL_WRITE, (long)buff, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_reflush(void) {
  invoke_syscall(SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mutex_init(int key) {
  return invoke_syscall(SYSCALL_LOCK_INIT, key, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_acquire(int mutex_idx) {
  invoke_syscall(SYSCALL_LOCK_ACQ, mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_release(int mutex_idx) {
  invoke_syscall(SYSCALL_LOCK_RELEASE, mutex_idx, IGNORE, IGNORE, IGNORE,
                 IGNORE);
}

long sys_get_timebase(void) {
  return invoke_syscall(SYSCALL_GET_TIMEBASE, IGNORE, IGNORE, IGNORE, IGNORE,
                        IGNORE);
}

long sys_get_tick(void) {
  return invoke_syscall(SYSCALL_GET_TICK, IGNORE, IGNORE, IGNORE, IGNORE,
                        IGNORE);
}

void sys_sleep(uint32_t time) {
  invoke_syscall(SYSCALL_SLEEP, time, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_strace(uint64_t strace_bitmask) {
  invoke_syscall(SYSCALL_STRACE, strace_bitmask, IGNORE, IGNORE, IGNORE,
                 IGNORE);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
  return invoke_syscall(SYSCALL_THREAD_CREATE, (long)thread, (long)attr,
                        (long)start_routine, (long)arg, IGNORE);
}

void pthread_yield(void) {
  invoke_syscall(SYSCALL_THREAD_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void pthread_exit(void *retval) {
  invoke_syscall(SYSCALL_THREAD_EXIT, (long)retval, IGNORE, IGNORE, IGNORE,
                 IGNORE);
}

pid_t sys_exec(char *name, int argc, char **argv) {
  invoke_syscall(SYSCALL_EXEC, (long)name, argc, (long)argv, IGNORE, IGNORE);
}

void sys_exit(void) {
  invoke_syscall(SYSCALL_EXIT, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_kill(pid_t pid) {
  return invoke_syscall(SYSCALL_KILL, pid, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_waitpid(pid_t pid) {
  return invoke_syscall(SYSCALL_WAITPID, pid, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_ps(void) {
  invoke_syscall(SYSCALL_PS, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

pid_t sys_getpid() {
  return invoke_syscall(SYSCALL_GETPID, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_getchar(void) {
  return invoke_syscall(SYSCALL_READCH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_barrier_init(int key, int goal) {
  return invoke_syscall(SYSCALL_BARR_INIT, key, goal, IGNORE, IGNORE, IGNORE);
}

void sys_barrier_wait(int bar_idx) {
  invoke_syscall(SYSCALL_BARR_WAIT, bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_barrier_destroy(int bar_idx) {
  invoke_syscall(SYSCALL_BARR_DESTROY, bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_condition_init(int key) {
  return invoke_syscall(SYSCALL_COND_INIT, key, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_wait(int cond_idx, int mutex_idx) {
  invoke_syscall(SYSCALL_COND_WAIT, cond_idx, mutex_idx, IGNORE, IGNORE,
                 IGNORE);
}

void sys_condition_signal(int cond_idx) {
  invoke_syscall(SYSCALL_COND_SIGNAL, cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_broadcast(int cond_idx) {
  invoke_syscall(SYSCALL_COND_BROADCAST, cond_idx, IGNORE, IGNORE, IGNORE,
                 IGNORE);
}

void sys_condition_destroy(int cond_idx) {
  invoke_syscall(SYSCALL_COND_DESTROY, cond_idx, IGNORE, IGNORE, IGNORE,
                 IGNORE);
}

int sys_semaphore_init(int key, int init) {
  return invoke_syscall(SYSCALL_SEMA_INIT, key, init, IGNORE, IGNORE, IGNORE);
}

void sys_semaphore_up(int sema_idx) {
  invoke_syscall(SYSCALL_SEMA_UP, sema_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_semaphore_down(int sema_idx) {
  invoke_syscall(SYSCALL_SEMA_DOWN, sema_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_semaphore_destroy(int sema_idx) {
  invoke_syscall(SYSCALL_SEMA_DESTROY, sema_idx, IGNORE, IGNORE, IGNORE,
                 IGNORE);
}

int sys_mbox_open(char *name) {
  return invoke_syscall(SYSCALL_MBOX_OPEN, (long)name, IGNORE, IGNORE, IGNORE,
                        IGNORE);
}

void sys_mbox_close(int mbox_id) {
  invoke_syscall(SYSCALL_MBOX_CLOSE, mbox_id, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length) {
  return invoke_syscall(SYSCALL_MBOX_SEND, mbox_idx, (long)msg, msg_length,
                        IGNORE, IGNORE);
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length) {
  return invoke_syscall(SYSCALL_MBOX_RECV, mbox_idx, (long)msg, msg_length,
                        IGNORE, IGNORE);
}
/************************************************************/