#include <type.h>
#include <assert.h>
#include <debugs.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/time.h>
#include <os/csr.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>

extern void user_trap_ret();

pcb_t pcb[NUM_MAX_TASK];
tcb_t tcb[NUM_MAX_THREADS];
switchto_context_t swtch_ctx[NUM_MAX_THREADS];
const ptr_t sched_kernel_stack = FREEMEM_KERNEL;
switchto_context_t sched_ctx;
tcb_t sched_tcb = {
    .status = TASK_READY,
    .tid = 0,
    .kernel_sp = (ptr_t)sched_kernel_stack,
    .ctx = &sched_ctx,
    .pcb = &sched_pcb,
};
pcb_t sched_pcb = {
    .pid = 0,
    .num_threads = 1,
    .name = "scheduler",
    .threads = {
        &sched_tcb.list,
        &sched_tcb.list,
    },
};

static int pcb_pool_queue[NUM_MAX_TASK];
static int pcb_pool_queue_head = 0;
static int pcb_pool_queue_tail = NUM_MAX_TASK - 1;
static int tcb_pool_queue[NUM_MAX_THREADS];
static int tcb_pool_queue_head = 0;
static int tcb_pool_queue_tail = NUM_MAX_THREADS - 1;
LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
tcb_t *volatile current_running;
tcb_t *volatile next_running;
/* global process id */
pid_t process_id = 1;

void init_pool() 
{
  for (int i = 0; i < NUM_MAX_TASK; i++)
    pcb_pool_queue[i] = i;
  for (int i = 0; i < NUM_MAX_THREADS; i++)
    tcb_pool_queue[i] = i;
}

pcb_t *alloc_pcb() 
{
  if (pcb_pool_queue_head == pcb_pool_queue_tail)
    return NULL;
  int pcb_offset = pcb_pool_queue[pcb_pool_queue_head];
  pcb_pool_queue_head = (pcb_pool_queue_head + 1) % NUM_MAX_TASK;
  return pcb + pcb_offset;
}

tcb_t *alloc_tcb() 
{
  if (tcb_pool_queue_head == tcb_pool_queue_tail)
    return NULL;
  int tcb_offset = tcb_pool_queue[tcb_pool_queue_head];
  tcb_pool_queue_head = (tcb_pool_queue_head + 1) % NUM_MAX_TASK;
  return tcb + tcb_offset;
}

static void push_tcb(tcb_t* t) 
{
  tcb_pool_queue_tail = (tcb_pool_queue_tail + 1) % NUM_MAX_THREADS;
  tcb_pool_queue[tcb_pool_queue_tail] = t - tcb;
}

tcb_t* new_tcb(pcb_t* p, ptr_t entry) 
{
  tcb_t* t = alloc_tcb();
  if (t == NULL)
    return NULL;
  t->ctx = swtch_ctx + (t - tcb);
  t->status = TASK_READY;
  t->tid = p->num_threads++;
  t->lock_bitmask = 0;
  t->wakeup_time = 0;
  t->sched_cnt = 0;
  t->kernel_sp = allocKernelPage(1);
  t->trapframe = (regs_context_t*)allocKernelPage(1);
  t->user_sp = allocUserPage(1);
  t->trapframe->kernel_sp = t->kernel_sp + PAGE_SIZE;
  t->trapframe->sstatus = SR_SPIE;
  t->trapframe->sp() = t->user_sp + PAGE_SIZE;
  t->trapframe->sepc = entry;
  t->ctx->ra = (uint64_t)user_trap_ret;
  t->ctx->sp = t->kernel_sp + PAGE_SIZE;
  t->pcb = p;
  t->list.next = t->list.prev = &(t->list);
  t->thread_list.next = t->thread_list.prev = &(t->thread_list);
  LIST_INSERT_TAIL(&(p->threads), &(t->thread_list)); 
  LIST_INSERT_TAIL(&ready_queue, &(t->list));
  return t;
}

pcb_t *new_pcb(const char *name, ptr_t entry) 
{
  pcb_t *p = alloc_pcb();
  if (p == NULL) {
    printl("Error: PCB is full!\n");
    return NULL;
  }
  p->pid = p - pcb + 1;
  // strcpy
  strcpy(p->name, name);
  p->num_threads = 0;
  p->threads.next = p->threads.prev = &(p->threads);
  if (new_tcb(p, entry) == NULL)
    return NULL;
  return p;
}

void dump_context(switchto_context_t *ctx) 
{
  printl("ra: %x\n", ctx->ra);
  printl("sp: %x\n", ctx->sp);
  printl("s0: %x\n", ctx->s0);
  printl("s1: %x\n", ctx->s1);
  printl("s2: %x\n", ctx->s2);
  printl("s3: %x\n", ctx->s3);
  printl("s4: %x\n", ctx->s4);
  printl("s5: %x\n", ctx->s5);
  printl("s6: %x\n", ctx->s6);
  printl("s7: %x\n", ctx->s7);
  printl("s8: %x\n", ctx->s8);
  printl("s9: %x\n", ctx->s9);
  printl("s10: %x\n", ctx->s10);
  printl("s11: %x\n", ctx->s11);
}

void dump_tcb(tcb_t *t) 
{
  printl("dumping thread:\n-------------------------------\n");
  printl("process: %s\n", t->pcb->name);
  printl("tid: %d\n", t->tid);
  printl("kernel_sp: %x\n", t->kernel_sp);
  printl("user_sp: %x\n", t->user_sp);
  printl("status: %s\n", task_status_str(t->status));
  dump_context(t->ctx);
  printl("holding locks:\n");
  for (int i = 0; i < LOCK_NUM; i++)
    if (HOLD_LOCK(t, i))
      dump_lock(i);
  printl("-------------------------------\n");
}

void dump_pcb(pcb_t *p) 
{
  printl("dumping process:\n-------------------------------\n");
  printl("pid: %d\n", p->pid);
  printl("name: %s\n", p->name);
  list_node_t *node = p->threads.next;
  while (node != &(p->threads)) {
    tcb_t *t = list_thread(node);
    dump_tcb(t);
    node = node->next;
  }
  printl("-------------------------------\n");
}

void dump_all_threads()
{
  printl("dumping threads in ready queue:\n");
  list_node_t *node = ready_queue.next;
  while (node != &ready_queue) {
    tcb_t *t = list_tcb(node);
    dump_tcb(t);
    node = node->next;
  }
}

void insert_tcb(list_head *queue, tcb_t *tcb) 
{
  LIST_INSERT_HEAD(queue, &(tcb->list));
}

int free_tcb(tcb_t *t) 
{
  pcb_t *p = t->pcb;
  assert_msg(t != &sched_tcb, "freeing the scheduler thread.");
  assert_msg(p->num_threads > 1, "freeing the last thread of the process.");
  LIST_REMOVE(&(t->thread_list));
  LIST_REMOVE(&(t->list));
  freeKernelPage((ptr_t)t->trapframe);
  freeKernelPage((ptr_t)t->kernel_sp);
  freeUserPage((ptr_t)t->user_sp);
  push_tcb(t);
  p->num_threads--;
  return 0;
}

void do_scheduler(void) 
{
  while (1) {
    check_sleeping();
    current_running = next_running;
    update_next();
    assert(current_running->status == TASK_READY);
    current_running->status = TASK_RUNNING;
    log_block(PROC, dump_tcb(current_running));
    current_running->sched_cnt++;
    timer_needs_reset = true;
    switch_to(&sched_ctx, current_running->ctx);
  }
}

static inline void sched(tcb_t *t) 
{
  switchto_context_t *current_ctx = t->ctx;
  current_running = &sched_tcb;
  switch_to(current_ctx, &sched_ctx);
}

void do_yield(void) 
{
  tcb_t *t = mythread();
  if (t->pcb->pid == 0)
    panic("scheduler is yielding!\n");
  log_line(PROC, "process %d (%s), thread %d is yielding\n", t->pcb->pid,
           t->pcb->name, t->tid);
  t->status = TASK_READY;
  sched(t);
}

void do_sleep(void) 
{
  uint32_t sleep_time;
  if (argint(0, (int *)&sleep_time) < 0)
    return;
  tcb_t *t = mythread();
  log_line(PROC, "%d: process %d (%s), thread %d is sleeping\n", get_timer(),
           t->pcb->pid, t->pcb->name, t->tid);
  t->wakeup_time = get_ticks() + sleep_time * time_base;
  do_block(&(t->list), &sleep_queue);
}

void do_block(list_node_t *tcb_node, list_head *queue) 
{
  tcb_t *t = list_tcb(tcb_node);
  log_line(PROC, "process %d (%s), thread %d is blocked\n", t->pcb->pid, t->pcb->name, t->tid);
  assert(t->status != TASK_BLOCKED);
  t->status = TASK_BLOCKED;
  LIST_REMOVE(tcb_node);
  LIST_INSERT_TAIL(queue, tcb_node);
  sched(t);
}

void do_unblock(list_node_t *tcb_node) 
{
  tcb_t *t = list_tcb(tcb_node);
  assert(t->status == TASK_BLOCKED);
  t->status = TASK_READY;
  log_line(PROC, "process %d (%s), thread %d is unblocked\n", t->pcb->pid, t->pcb->name, t->tid);
  LIST_REMOVE(tcb_node);
  // a special case
  // if the current_running is at the tail of the queue
  // then the next_running should be the head of the queue
  // in this case we update the next_running as the new unblocked thread
  // so that in the new unblocked process will be runned immediately
  // otherwise, the thread will still wait for a round of scheduling
  if (current_running->list.next == &ready_queue)
    next_running = list_tcb(tcb_node);
  LIST_INSERT_TAIL(&ready_queue, tcb_node);
}

void do_thread_create() 
{
  tcb_t* t = mythread();
  pcb_t* p = t->pcb;
  ptr_t thread_ptr = argraw(0);
  ptr_t thread_attr_ptr = argraw(1); 
  ptr_t routine_ptr = argraw(2);
  ptr_t arg_ptr = argraw(3);
  tcb_t* nt = new_tcb(p, routine_ptr);
  if (nt == NULL) {
    t->trapframe->a0() = -1;
    return ;
  }
  nt->trapframe->a0() = arg_ptr;
  *(thread_t*)thread_ptr = nt - tcb;
  t->trapframe->a0() = 0;
  return ;
}

void do_thread_yield(void) {
  tcb_t *t = mythread();
  do_yield();
  t->trapframe->regs[10] = 0;
}

void do_thread_exit() 
{
  tcb_t* t = mythread();
  ptr_t retval = argraw(0);
  if (free_tcb(t) != 0) {
    *(int*)retval = -1;
    return ;
  }
  do_scheduler();
  if ((int*)retval != NULL)
    *(int*)retval = 0;
}

void do_sched_times()
{
  tcb_t* t = mythread();
  tcb_t* thread = &tcb[argraw(0)];
  t->trapframe->a0() = thread->sched_cnt;
}