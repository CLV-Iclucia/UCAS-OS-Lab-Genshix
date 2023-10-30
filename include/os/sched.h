/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Copyright (C) 2018 Institute of Computing Technology, CAS Author :
 * Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * *
 * * * * * * Process scheduling related content, such as: scheduler, process
 * blocking, process wakeup, process creation, process kill, etc.
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

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <os/list.h>
#include <os/lock.h>
#include <os/mm.h>
#include <type.h>
#include <assert.h>

#define NUM_MAX_TASK 16
#define NAME_MAXLEN 16
#define NUM_MAX_THREADS NUM_MAX_TASK * 4
/* used to save register infomation */
typedef struct regs_context {
  /* Saved main processor registers.*/
  reg_t regs[32];

  /* Saved special registers. */
  reg_t sstatus;
  reg_t sepc;
  reg_t sbadaddr;
  reg_t scause;
  /* I thought about "how to get kernel sp elegantly" for ages*/
  /* And later I found that even xv6 did this in a very ugly way( */
  uint64_t kernel_sp;
} regs_context_t;

#define zero() regs[0]
#define ra() regs[1]
#define sp() regs[2]
#define gp() regs[3]
#define tp() regs[4]
#define t0() regs[5]
#define t1() regs[6]
#define t2() regs[7]
#define s0() regs[8]
#define s1() regs[9]
#define a0() regs[10]
#define a1() regs[11]
#define a2() regs[12]
#define a3() regs[13]
#define a4() regs[14]
#define a5() regs[15]
#define a6() regs[16]
#define a7() regs[17]
#define s2() regs[18]
#define s3() regs[19]
#define s4() regs[20]
#define s5() regs[21]
#define s6() regs[22]
#define s7() regs[23]
#define s8() regs[24]
#define s9() regs[25]
#define s10() regs[26]
#define s11() regs[27]
#define t3() regs[28]
#define t4() regs[29]
#define t5() regs[30]
#define t6() regs[31]

/* used to save register infomation in switch_to */
typedef struct switchto_context {
  /* Callee saved registers.*/
  reg_t ra;
  reg_t sp;
  reg_t s0;
  reg_t s1;
  reg_t s2;
  reg_t s3;
  reg_t s4;
  reg_t s5;
  reg_t s6;
  reg_t s7;
  reg_t s8;
  reg_t s9;
  reg_t s10;
  reg_t s11;
} switchto_context_t;

typedef enum {
  TASK_BLOCKED,
  TASK_RUNNING,
  TASK_READY,
  TASK_EXITED,
} task_status_t;

static inline const char *task_status_str(task_status_t status) {
  switch (status) {
  case TASK_BLOCKED:
    return "BLOCKED";
  case TASK_RUNNING:
    return "RUNNING";
  case TASK_READY:
    return "READY";
  case TASK_EXITED:
    return "EXITED";
  default:
    return "UNKNOWN";
  }
}

/* Process Control Block */
typedef struct pcb {
  list_node_t threads;
  int num_threads;
  /* process id */
  pid_t pid;
  char name[NAME_MAXLEN];
} pcb_t;

/* Thread Control Block */
typedef struct tcb {
  regs_context_t *trapframe;
  /* register context */
  // NOTE: this order must be preserved, which is defined in regs.h!!
  reg_t kernel_sp;
  reg_t user_sp;
  /* previous, next pointer in ready_queue */
  list_node_t list;
  list_node_t thread_list;
  /* BLOCK | READY | RUNNING */
  task_status_t status;
  int tid;
  /* cursor position */
  int cursor_x;
  int cursor_y;
  int sched_cnt;
  /* time(seconds) to wake up sleeping PCB */
  uint64_t wakeup_time;
  switchto_context_t *ctx;
  uint64_t strace_bitmask; // what syscalls to trace?
  uint64_t lock_bitmask;   // locks held by this thread
  pcb_t *pcb;              // the process this thread belongs to
} tcb_t;

typedef uint64_t thread_t;

#define offsetof(type, member) ((size_t)(&((type *)0)->member))
#define HOLD_LOCK(thread, idx) ((thread)->lock_bitmask & LOCK_MASK(idx))
// given a pointer to a list_node_t, get the pcb_t that contains it
static inline tcb_t *list_tcb(list_node_t *ptr) {
  return (tcb_t *)((char *)ptr - offsetof(tcb_t, list));
}
static inline tcb_t *list_thread(list_node_t *ptr) {
  return (tcb_t *)((char *)ptr - offsetof(tcb_t, thread_list));
}
/* ready queue to run */
extern list_head ready_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

/* current running task PCB */
extern tcb_t *volatile current_running;
extern tcb_t *volatile next_running;
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];
extern tcb_t tcb[NUM_MAX_TASK * 4];
extern tcb_t sched_tcb;
extern pcb_t sched_pcb;
extern const ptr_t sched_stack;

extern void switch_to(switchto_context_t *prev, switchto_context_t *next);
// a inline func myproc to get the current_running
static inline tcb_t *volatile mythread() { return current_running; }
void init_pool();
void do_scheduler(void);
void do_sleep(void);
void do_yield(void);
void do_block(list_node_t *, list_head *queue);
void do_unblock(list_node_t *);
tcb_t *alloc_tcb();
pcb_t *alloc_pcb();
tcb_t *new_tcb(pcb_t *p, ptr_t entry);
pcb_t *new_pcb(const char *name, ptr_t entry);

static inline void update_next() 
{
  if (current_running->list.next == &ready_queue)
    next_running = list_tcb(ready_queue.next);
  else
    next_running = list_tcb(current_running->list.next);
}

extern int free_tcb(tcb_t *t);

static inline tcb_t* main_thread(pcb_t *p) 
{
  return list_thread(p->threads.next);
}
void insert_tcb(list_head *queue, tcb_t *tcb);
void dump_all_threads();
void dump_context(switchto_context_t *ctx);
void dump_pcb(pcb_t *p);
void do_thread_create(void);
void do_thread_exit(void);
void do_thread_yield(void);
void do_sched_times(void);
/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

#endif
