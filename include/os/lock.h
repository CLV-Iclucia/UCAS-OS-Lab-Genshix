/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Copyright (C) 2018 Institute of Computing Technology, CAS Author :
 * Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Thread Lock
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * */

#ifndef INCLUDE_LOCK_H_
#define INCLUDE_LOCK_H_

#include <os/list.h>
#include <os/smp.h>

#define LOCK_NUM 16
#define CPU_NUM 2
// bitmask for all the locks
#define LOCK_MASK_ALL 0xffff
#define LOCK_MASK_NONE 0x0000
#define LOCK_MASK(idx) (1ll << (idx))
typedef enum {
  UNLOCKED,
  LOCKED,
} lock_status_t;

static inline const char *lock_status_str(lock_status_t status) {
  if (status == UNLOCKED)
    return "UNLOCKED";
  else
    return "LOCKED";
}

typedef struct spin_lock {
  int cpuid;
  volatile lock_status_t status;
} spin_lock_t;

// all the constraints for lock:
// 1. the block_queue cannot be broken
// 2. the block_queues of different locks cannot overlap
// 3. all the process in the block_queue cannot hold this lock
// 4. lock can either be UNLOCKED or LOCKED
typedef struct mutex_lock {
  spin_lock_t lock;  // this lock locks not only the mutex_lock_t, but also the
                     // block_queue
  list_head block_queue;
  int key;
  lock_status_t status;
} mutex_lock_t;

void init_locks(void);
void spin_lock_init(spin_lock_t *lock);
int spin_lock_try_acquire(spin_lock_t *lock);
void spin_lock_acquire(spin_lock_t *lock);
void spin_lock_release(spin_lock_t *lock);

static inline volatile bool spin_lock_holding(spin_lock_t *lock) {
  return lock->status == LOCKED && lock->cpuid == mycpuid();
}

static inline bool holding_no_spin_lock() {
  return mycpu()->spin_lock_cnt == 0;
}

int do_mutex_lock_init(int key);
void do_mutex_lock_acquire(int mlock_idx);
void do_mutex_lock_release(int mlock_idx);
void do_mutex_init(void);
void do_mutex_acquire(void);
void do_mutex_release(void);

typedef struct tcb tcb_t;
/************************************************************/
typedef struct barrier {
  spin_lock_t lock;
  list_head queue;
  uint32_t num;
  int key;
  int goal;
} barrier_t;

#define BARRIER_NUM 16

void init_barriers(void);
int do_barrier_init(int key, int goal);
void do_barrier_wait(int bar_idx);
void do_barrier_destroy(int bar_idx);
void do_barr_init(void);
void do_barr_wait(void);
void do_barr_destroy(void);

typedef struct condition {
  spin_lock_t lock;
  int key;
  list_head queue;
} condition_t;

#define CONDITION_NUM 16

void init_conditions(void);
int do_condition_init(int key);
void do_condition_wait(int cond_idx, int mutex_idx);
void do_condition_signal(int cond_idx);
void do_condition_broadcast(int cond_idx);
void do_condition_destroy(int cond_idx);
void do_cond_init(void);
void do_cond_wait(void);
void do_cond_signal(void);
void do_cond_broadcast(void);
void do_cond_destroy(void);

typedef struct semaphore {
  spin_lock_t lock;
  int key;
  int value;
  list_head queue;
} semaphore_t;

#define SEMAPHORE_NUM 16

void init_semaphores(void);
int do_semaphore_init(int key, int init);
void do_semaphore_up(int sema_idx);
void do_semaphore_down(int sema_idx);
void do_semaphore_destroy(int sema_idx);
void do_sema_init(void);
void do_sema_up(void);
void do_sema_down(void);
void do_sema_destroy(void);

#define MAX_MBOX_LENGTH (64)
#define MAX_NAME_LENGTH (32)
typedef struct mailbox {
  spin_lock_t lock;
  int length;
  int head;
  int tail;
  int buf[MAX_MBOX_LENGTH];
  char name[MAX_NAME_LENGTH];
  list_head send_queue;
  list_head recv_queue;
} mailbox_t;

#define MBOX_NUM 16
void init_mbox();
int do_mbox_open(char *name);
void do_mbox_close(int mbox_idx);
int do_mbox_send(int mbox_idx, void *msg, int msg_length);
int do_mbox_recv(int mbox_idx, void *msg, int msg_length);
void do_mailbox_open(void);
void do_mailbox_close(void);
void do_mailbox_send(void);
void do_mailbox_recv(void);
/************************************************************/

#endif
