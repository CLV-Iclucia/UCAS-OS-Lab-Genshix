#include <assert.h>
#include <debugs.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/time.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>
pcb_t pcb[NUM_MAX_TASK];
switchto_context_t swtch_ctx[NUM_MAX_TASK];
const ptr_t sched_kernel_stack = FREEMEM_KERNEL;
const ptr_t sched_user_stack = FREEMEM_USER;
switchto_context_t sched_ctx;
pcb_t sched_pcb = {
    .name = "scheduler",
    .status = TASK_READY,
    .pid = 0,
    .kernel_sp = (ptr_t)sched_kernel_stack,
    .user_sp = (ptr_t)sched_user_stack,
    .ctx = &sched_ctx,
};
static int pcb_pool_queue[NUM_MAX_TASK];
static int pcb_pool_queue_head = 0;
static int pcb_pool_queue_tail = NUM_MAX_TASK - 1;
LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t *volatile current_running;
pcb_t *volatile next_running;
/* global process id */
pid_t process_id = 1;

void init_pcb_pool() {
  for (int i = 0; i < NUM_MAX_TASK; i++)
    pcb_pool_queue[i] = i;
}

pcb_t *alloc_pcb() {
  if (pcb_pool_queue_head == pcb_pool_queue_tail)
    return NULL;
  int pcb_offset = pcb_pool_queue[pcb_pool_queue_head];
  pcb_pool_queue_head = (pcb_pool_queue_head + 1) % NUM_MAX_TASK;
  return pcb + pcb_offset;
}

pcb_t *new_pcb(const char *name) {
  pcb_t *p = alloc_pcb();
  if (p == NULL) {
    printl("Error: PCB is full!\n");
    return NULL;
  }
  p->pid = process_id++;
  // avoid too large pid
  if (process_id == 0x7FFFFFFF)
    process_id = 1;
  // strcpy
  strcpy(p->name, name);
  // pcb->kernel_sp = allocKernelPage(1);
  // pcb->user_sp = allocUserPage(1);
  p->list.next = NULL;
  p->list.prev = NULL;
  p->ctx = swtch_ctx + (p - pcb);
  return p;
}

void dump_context(switchto_context_t *ctx) {
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

void dump_pcb(pcb_t *p) {
  printl("dumping process:\n-------------------------------\n");
  printl("pid: %d\n", p->pid);
  printl("name: %s\n", p->name);
  printl("kernel_sp: %x\n", p->kernel_sp);
  printl("user_sp: %x\n", p->user_sp);
  printl("status: %s\n", task_status_str(p->status));
  dump_context(p->ctx);
  printl("holding locks:\n");
  for (int i = 0; i < LOCK_NUM; i++)
    if (HOLD_LOCK(p, i))
      dump_lock(i);
  printl("-------------------------------\n");
}

void dump_all_proc() {
  printl("dumping processes in ready queue:\n");
  // print the info of processes
  list_node_t *node = ready_queue.next;
  while (node != &ready_queue) {
    pcb_t *p = list_pcb(node);
    dump_pcb(p);
    node = node->next;
  }
}

void insert_pcb(list_head *queue, pcb_t *pcb) 
{
  LIST_INSERT_HEAD(queue, &(pcb->list));
}

void do_scheduler(void) 
{
  while (1) {
    check_sleeping();
    current_running = next_running;
    if (current_running->list.next == &ready_queue)
      next_running = list_pcb(ready_queue.next);
    else
      next_running = list_pcb(current_running->list.next);
    assert(current_running->status == TASK_READY);
    current_running->status = TASK_RUNNING;
    //    log_block(PROC, dump_pcb(current_running));
    switch_to(&sched_ctx, current_running->ctx);
  }
}

static inline void sched(pcb_t* p) {
  switchto_context_t* current_ctx = p->ctx;
  current_running = &sched_pcb;
  switch_to(current_ctx, &sched_ctx);
}

void do_yield(void) {
  pcb_t *p = myproc();
  if (p->pid == 0)
    panic("scheduler is yielding!\n");
  log_line(PROC, "process %d (%s) is yielding\n", p->pid, p->name);
  p->status = TASK_READY;
  sched(p);
}

void do_sleep(void) {
  uint32_t sleep_time;
  if (argint(0, (int *)&sleep_time) < 0)
    return;
  pcb_t *p = myproc();
  log_line(PROC, "%d: process %d (%s) is sleeping\n", get_timer(), p->pid,
           p->name);
  p->wakeup_time = get_ticks() + sleep_time * time_base;
  do_block(&(p->list), &sleep_queue);
}

void do_block(list_node_t *pcb_node, list_head *queue) {
  pcb_t *p = list_pcb(pcb_node);
  log_line(PROC, "process %d (%s) is blocked\n", p->pid, p->name);
  assert(p->status != TASK_BLOCKED);
  p->status = TASK_BLOCKED;
  LIST_REMOVE(pcb_node);
  LIST_INSERT_TAIL(queue, pcb_node);
  sched(p);
}

void do_unblock(list_node_t *pcb_node) {
  pcb_t *p = list_pcb(pcb_node);
  assert(p->status == TASK_BLOCKED);
  p->status = TASK_READY;
  log_line(PROC, "%d process %d (%s) is unblocked", get_timer(), p->pid,
           p->name);
  LIST_REMOVE(pcb_node);
  // a special case
  // if the current_running is at the tail of the queue
  // then the next_running should be the head of the queue
  // in this case we update the next_running as the new unblocked process
  // so that in the new unblocked process will be runned immediately
  // otherwise, the process will still wait for a round of scheduling
  if (current_running->list.next == &ready_queue)
    next_running = list_pcb(pcb_node);
  LIST_INSERT_TAIL(&ready_queue, pcb_node);
}