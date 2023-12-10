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

#include <os/smp.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>
#include <assert.h>

#define NUM_MAX_TASK 32
#define NAME_MAXLEN 16
#define NUM_MAX_THREADS NUM_MAX_TASK * 4
#define NUM_MAX_THREADS_PER_TASK 64

// constraints for a valid trapframe:
// 1. sepc, ra and sp must be aligned and within valid ranges
// 2. 
typedef struct regs_context {
  /* Saved main processor registers.*/
  reg_t regs[32];

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

typedef enum {
  PROC_ACTIVATE,
  PROC_INACTIVATE,
} proc_status_t;

/* Process Control Block */
typedef struct pcb {
  char name[NAME_MAXLEN]; // read only
  spin_lock_t lock;
  // constraints for thread lists:
  // 1. num_threads == list_length(&threads)
  // 2. threads cannot be broken
  list_node_t threads;
  int num_threads;
  uint64_t tid_mask;
  uint64_t filesz;
  uint64_t size;          
  proc_status_t status;
  spin_lock_t wait_lock;
  list_node_t wait_queue;
  struct pcb *parent;     // read only
  PTE* pgdir;              // read only
  /* process id */
  pid_t pid;              // read only
  uint32_t taskset;
} pcb_t;

/* Thread Control Block */
typedef struct tcb {
  regs_context_t *trapframe;
  kva_t kstack; // the bottom of the stack!
  // the uva of ustack can be calculated by STACK_TOP_UVA - (tid + 1) * PAGE_SIZE
  // so we store the kva of ustack here
  kva_t ustack; // the bottom of the stack!
  spin_lock_t lock;
  spin_lock_t join_lock;
  // constraints for the two lists: cannot be broken
  list_node_t list; 
  list_node_t thread_list;
  list_node_t join_queue; // the queue of threads waiting for this thread to exit
  list_node_t* current_queue;
  task_status_t status;
  // constraints: the tids of all threads in the same process must be unique
  int tid;
  int cursor_x;
  int cursor_y;
  uint64_t wakeup_time;
  switchto_context_t *ctx;
  uint64_t strace_bitmask; // what syscalls to trace?
  // constraints: lock_bitmasks cannot overlap
  uint64_t lock_bitmask;   // locks held by this thread
  pcb_t *pcb;              // the process this thread belongs to
  uint8_t cpuid;
} tcb_t;

typedef uint32_t thread_t;

#define offsetof(type, member) ((size_t)(&((type *)0)->member))
#define HOLD_LOCK(thread, idx) ((thread)->lock_bitmask & LOCK_MASK(idx)) // must be called with lock held

static inline tcb_t *list_tcb(list_node_t *ptr) {
  return (tcb_t *)((char *)ptr - offsetof(tcb_t, list));
}
static inline tcb_t *list_thread(list_node_t *ptr) {
  return (tcb_t *)((char *)ptr - offsetof(tcb_t, thread_list));
}


// constraints:
// 1. the ready_queue cannot be broken
// 2. the ready_queue cannot overlap with the sleep_queue and the block_queues of all locks
extern list_head ready_queue;
extern spin_lock_t ready_queue_lock;

extern tcb_t sched_tcb[NR_CPUS];
extern pcb_t sched_pcb[NR_CPUS];
extern switchto_context_t sched_ctx[NR_CPUS];

extern pcb_t pcb[NUM_MAX_TASK];
extern tcb_t tcb[NUM_MAX_TASK * 4];
extern const ptr_t sched_stack;

extern void switch_to(switchto_context_t *prev, switchto_context_t *next);
// a inline func mythread to get the current_running
static inline tcb_t *volatile mythread() { return mycpu()->current_running; }
static inline pcb_t *volatile myproc() { return mythread()->pcb; }
static inline bool running_on_scheduler() 
{
  int cpuid = mycpuid();
  return cpus[cpuid].current_running == &sched_tcb[cpuid];
}
void init_pool();
void do_scheduler(void);
void do_sleep(void);
void do_yield(void);
void do_block(list_node_t *, list_head *queue, spin_lock_t *queue_lock);
void do_unblock(list_node_t *);
tcb_t *alloc_tcb();
pcb_t *alloc_pcb();
tcb_t *new_tcb(pcb_t *p, uva_t entry_uva);
pcb_t *new_pcb(const char *name);
int kill_pcb(pcb_t *p);
int kill_tcb(tcb_t *t);

extern int free_tcb(tcb_t *t);

static inline tcb_t* main_thread(pcb_t *p) 
{
  assert(spin_lock_holding(&p->lock));
  return list_thread(p->threads.next);
}
// atomically insert a tcb into ready queue
static inline void ready_queue_insert(tcb_t* t) {
  spin_lock_acquire(&ready_queue_lock);
  LIST_INSERT_TAIL(&ready_queue, &t->list);
  t->current_queue = &ready_queue;
  spin_lock_release(&ready_queue_lock);
}
// must be called with t->lock held, but will exit with NO LOCK
// what sched do: update queue, current cpu and switch context
// NOTE: the update of the status of tcb should be done before calling this
static inline void sched(tcb_t *t) 
{
  assert(spin_lock_holding(&t->lock));
  switchto_context_t *current_ctx = t->ctx;
  cpu_t *c = mycpu();
  c->current_running = sched_tcb + c->id;
  // we only put new nodes at the tail of the ready_queue
  // and if the thread is blocked, it should lie in other queues so we do nothing
  if (t->status != TASK_BLOCKED)
    ready_queue_insert(t);
  t->cpuid = -1;
  spin_lock_release(&t->lock);
  switch_to(current_ctx, c->sched_ctx);
  assert(holding_no_spin_lock());
}

// must hold the lock of t
static inline void prepare_sched(tcb_t* t) {
  assert(spin_lock_holding(&t->lock));
  t->status = TASK_READY;
  assert(t->current_queue == NULL);
  ready_queue_insert(t);
}

// atomically takes and removes the head of the queue, NULL if empty
static inline tcb_t* ready_queue_pop() {
  spin_lock_acquire(&ready_queue_lock);
  if (LIST_EMPTY(ready_queue)) {
    spin_lock_release(&ready_queue_lock);
    return NULL;
  }
  tcb_t* t = NULL;
  list_node_t* head = LIST_FIRST(ready_queue);
  t = list_tcb(head);
  t->current_queue = NULL;
  LIST_REMOVE(head);
  spin_lock_release(&ready_queue_lock);
  return t;
}

static inline pcb_t* get_pcb(int pid) {
  assert(pid != -1);
  return pcb + pid - 1;
}

static inline bool is_valid_uva(uva_t uva, pcb_t *p) {
  spin_lock_acquire(&p->lock);
  bool valid = uva.addr >= USER_ENTRYPOINT_UVA;
  spin_lock_release(&p->lock);
  return valid;
}

static inline uint64_t argraw(int n) {
  regs_context_t *r = mythread()->trapframe;
  assert(n >= 0 && n < 6);
  switch(n) {
    case 0: return r->a0();
    case 1: return r->a1();
    case 2: return r->a2();
    case 3: return r->a3();
    case 4: return r->a4();
    case 5: return r->a5();
  }
  return -1;
}

static inline int argint(int n, int *ip) {
  *ip = argraw(n);
  return 0;
}

void insert_tcb(list_head *queue, tcb_t *tcb);
void dump_all_threads();
void dump_context(switchto_context_t *ctx);
void dump_pcb(pcb_t *p);
void do_thread_create(void);
void do_thread_exit(void);
void do_thread_yield(void);
void do_thread_join(void);
/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/* TODO [P3-TASK1] exec exit kill waitpid ps*/
extern void do_exec(void);
extern void do_exit(void);
extern void do_kill(void);
extern void do_waitpid(void);
extern void do_ps(void);
extern void do_getpid(void);
extern void do_taskset(void);
extern void do_fork(void);

/************************************************************/

#endif
