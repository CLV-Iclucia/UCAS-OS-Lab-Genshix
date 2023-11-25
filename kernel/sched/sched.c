#include <assert.h>
#include <debugs.h>
#include <os/csr.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/time.h>
#include <os/loader.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>
#include <type.h>

extern void user_trap_ret();

pcb_t pcb[NUM_MAX_TASK];
tcb_t tcb[NUM_MAX_THREADS];
switchto_context_t swtch_ctx[NUM_MAX_THREADS];
switchto_context_t sched_ctx[NR_CPUS];

tcb_t sched_tcb[NR_CPUS] = {
    {
        .status = TASK_READY,
        .tid = 0,
        .kernel_sp = (ptr_t)(FREEMEM_KERNEL - 2 * PAGE_SIZE),
        .ctx = &sched_ctx[0],
        .cursor_x = 0,
        .cursor_y = 0,
        .lock = {
          .status = UNLOCKED,
        },
        .pcb = &sched_pcb[0],
    },
    {
        .status = TASK_READY,
        .tid = 0,
        .kernel_sp = (ptr_t)(FREEMEM_KERNEL - PAGE_SIZE),
        .ctx = &sched_ctx[1],
        .cursor_x = 0,
        .cursor_y = 1,
        .lock = {
          .cpuid = -1,
          .status = UNLOCKED,
        },
        .pcb = &sched_pcb[1],
    }};
pcb_t sched_pcb[NR_CPUS] = {
    {
        .status = PROC_ACTIVATE,
        .pid = 0,
        .num_threads = 1,
        .threads = {
            .next = &(sched_tcb[0].thread_list),
            .prev = &(sched_tcb[0].thread_list),
        },
        .lock = {
          .status = UNLOCKED,
        },
    },
    {
        .status = PROC_ACTIVATE,
        .pid = 0,
        .num_threads = 1,
        .threads = {
            .next = &(sched_tcb[1].thread_list),
            .prev = &(sched_tcb[1].thread_list),
        },
        .lock = {
          .cpuid = -1,
          .status = UNLOCKED,
        }
    }};

static int pcb_pool_queue[NUM_MAX_TASK];
static int pcb_pool_queue_head = 0;
static int pcb_pool_queue_tail = NUM_MAX_TASK - 1;
static spin_lock_t pcb_pool_lock;
static int tcb_pool_queue[NUM_MAX_THREADS];
static int tcb_pool_queue_head = 0;
static int tcb_pool_queue_tail = NUM_MAX_THREADS - 1;
static spin_lock_t tcb_pool_lock;

LIST_HEAD(ready_queue);
spin_lock_t ready_queue_lock;

/* current running task PCB */
tcb_t *volatile current_running[NR_CPUS];

void init_pool() {
  for (int i = 0; i < NUM_MAX_TASK; i++)
    pcb_pool_queue[i] = i;
  pcb_pool_lock.status = UNLOCKED;
  for (int i = 0; i < NUM_MAX_THREADS; i++)
    tcb_pool_queue[i] = i;
  tcb_pool_lock.status = UNLOCKED;
}

// no need to hold lock, this can be done atomically
pcb_t *alloc_pcb() {
  spin_lock_acquire(&pcb_pool_lock);
  if (pcb_pool_queue_head == pcb_pool_queue_tail)
    return NULL;
  int pcb_offset = pcb_pool_queue[pcb_pool_queue_head];
  pcb_pool_queue_head = (pcb_pool_queue_head + 1) % NUM_MAX_TASK;
  spin_lock_release(&pcb_pool_lock);
  return pcb + pcb_offset;
}

// no need to hold lock, this can be done atomically
tcb_t *alloc_tcb() {
  spin_lock_acquire(&tcb_pool_lock);
  if (tcb_pool_queue_head == tcb_pool_queue_tail)
    return NULL;
  int tcb_offset = tcb_pool_queue[tcb_pool_queue_head];
  tcb_pool_queue_head = (tcb_pool_queue_head + 1) % NUM_MAX_TASK;
  spin_lock_release(&tcb_pool_lock);
  return tcb + tcb_offset;
}

// no need to hold lock, this can be done atomically
static void push_tcb(tcb_t *t) {
  spin_lock_acquire(&tcb_pool_lock);
  tcb_pool_queue_tail = (tcb_pool_queue_tail + 1) % NUM_MAX_THREADS;
  tcb_pool_queue[tcb_pool_queue_tail] = t - tcb;
  spin_lock_release(&tcb_pool_lock);
}

static void push_pcb(pcb_t *p) {
  spin_lock_acquire(&pcb_pool_lock);
  pcb_pool_queue_tail = (pcb_pool_queue_tail + 1) % NUM_MAX_TASK;
  pcb_pool_queue[pcb_pool_queue_tail] = p - pcb;
  spin_lock_release(&pcb_pool_lock);
}

// must be called with the lock of pcb held
// this function will return with the lock of the new tcb HELD!
// 1. alloc new tcb and acquire the lock of the new tcb
// 1. release the lock of the queue of pcb
// 2. acquire the lock of the ready queue
// 3. release the lock of the ready queue
// note: this will not put the tcb to running, and will not set the args!
tcb_t *new_tcb(pcb_t *p, ptr_t entry) {
  assert(spin_lock_holding(&p->lock));
  tcb_t *t = alloc_tcb();
  if (t == NULL)
    return NULL;
  spin_lock_init(&t->lock);
  spin_lock_acquire(&(t->lock));
  t->ctx = swtch_ctx + (t - tcb);
  t->tid = p->num_threads++;
  t->lock_bitmask = 0;
  t->wakeup_time = 0;
  t->kernel_sp = allocKernelPage(1);
  t->trapframe = (regs_context_t *)allocKernelPage(1);
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
  return t;
}

// no need to hold lock, this can be done atomically
// but will exit with the lock of the pcb HELD!
pcb_t *new_pcb(const char *name, ptr_t entry) {
  pcb_t *p = alloc_pcb();
  if (p == NULL) {
    printl("Error: PCB is full!\n");
    return NULL;
  }
  spin_lock_init(&(p->lock));
  spin_lock_acquire(&p->lock);
  p->pid = p - pcb + 1;
  p->status = PROC_ACTIVATE;
  strcpy(p->name, name);
  p->num_threads = 0;
  p->threads.next = p->threads.prev = &(p->threads);
  tcb_t *nt = new_tcb(p, entry);
  if (nt == NULL) {
    return NULL;
  }
  spin_lock_release(&nt->lock);
  return p;
}

void insert_tcb(list_head *queue, tcb_t *tcb) {
  LIST_INSERT_HEAD(queue, &(tcb->list));
}

// must be called with the lock for t AND the lock for thread queue held
// and can only be called when the cpu is running on the scheduler
int free_tcb(tcb_t *t) {
  assert(running_on_scheduler());
  pcb_t *p = t->pcb;
  assert(spin_lock_holding(&p->lock));
  assert(t->current_queue == NULL);
  assert_msg(t != &sched_tcb[mycpuid()], "freeing the scheduler thread.");
  LIST_REMOVE(&(t->thread_list));
  p->num_threads--;
  freeKernelPage((ptr_t)t->trapframe);
  freeKernelPage((ptr_t)t->kernel_sp);
  freeUserPage((ptr_t)t->user_sp);
  spin_lock_release(&t->lock);
  // it is fine here, since at this time the status is already TASK_EXITED
  push_tcb(t);
  return 0;
}

// must be called with the lock of p held
int kill_pcb(pcb_t* p) 
{
  assert(spin_lock_holding(&p->lock));
  // run through all the threads
  list_node_t* node = p->threads.next;
  p->status = PROC_INACTIVATE;
  while (node != &(p->threads)) {
    tcb_t* t = list_tcb(node);
    spin_lock_acquire(&t->lock);
    t->status = TASK_EXITED;
    spin_lock_release(&t->lock);
    node = node->next;
  }
  spin_lock_acquire(&p->wait_lock);
  node = LIST_FIRST(p->wait_queue);
  while (node != &(p->wait_queue)) {
    do_unblock(node);
    node = node->next;
  }
  spin_lock_release(&p->wait_lock);
  return 0;
}

int kill_tcb(tcb_t* t)
{
  assert(spin_lock_holding(&t->lock));
  t->status = TASK_EXITED;
  return 0;
}

void free_pcb(pcb_t* p)
{
  assert(spin_lock_holding(&p->lock));
  assert(LIST_EMPTY(p->threads) && p->num_threads == 0);
  freeUserPages(p->addr, p->size);
  spin_lock_release(&p->lock);
  push_pcb(p);
}

// hold no locks when entering this loop
// in this function, we also free exited tcb and pcb (if all of its tcb have exited)
// because the scheduler run on another kernel stack 
// we can free the kernel stack of tcb safely
void do_scheduler(void) {
  while (1) {
    assert(holding_no_spin_lock());
    cpu_t *c = mycpu();
    check_sleeping();
    tcb_t *t = ready_queue_pop();
    if (t == NULL) continue;
    pcb_t* p = t->pcb;
    spin_lock_acquire(&p->lock);
    spin_lock_acquire(&t->lock);
    if (t->status == TASK_EXITED) {
      free_tcb(t);
      if (p->num_threads == 0)
        free_pcb(p);
      continue;
    }
    t->status = TASK_RUNNING;
    spin_lock_release(&t->lock);
    spin_lock_release(&p->lock);
    c->current_running = t;
    c->timer_needs_reset = true;
    switch_to(c->sched_ctx, t->ctx);
  }
}

// no need to hold lock to enter
// this function will acquire the lock of the thread and release it after sched
// back
void do_yield(void) {
  assert(holding_no_spin_lock());
  tcb_t *t = mythread();
  spin_lock_acquire(&t->lock);
  if (t->pcb->pid == 0)
    panic("scheduler is yielding!\n");
  log_line(PROC, "process %d (%s), thread %d is yielding\n", t->pcb->pid,
           t->pcb->name, t->tid);
  t->status = TASK_READY;
  sched(t);
}

// this function will acquire the lock of the thread and the lock of the queue
// before blocking the lock of the thread will still be held after returning
// from this function
void do_sleep(void) {
  uint32_t sleep_time;
  if (argint(0, (int *)&sleep_time) < 0)
    return;
  cpu_t *c = mycpu();
  tcb_t *t = c->current_running;
  t->wakeup_time = get_ticks() + sleep_time * time_base;
  spin_lock_acquire(&t->lock);
  do_block(&(t->list), &c->sleep_queue, NULL);
}

// must called with the lock for queue and the lock of tcb held
// the lock for queue will be released in this function
// will first acquire the lock for the ready queue and later release it
void do_block(list_node_t *tcb_node, list_head *queue,
              spin_lock_t *queue_lock) {
  tcb_t *t = list_tcb(tcb_node);
  assert(spin_lock_holding(&t->lock));
  assert(queue_lock == NULL || spin_lock_holding(queue_lock));
  log_line(PROC, "process %d (%s), thread %d is blocked\n", t->pcb->pid,
           t->pcb->name, t->tid);
  assert(t->status != TASK_BLOCKED);
  t->status = TASK_BLOCKED;
  LIST_INSERT_TAIL(queue, tcb_node);
  t->current_queue = queue;
  if (queue_lock != NULL)
    spin_lock_release(queue_lock);
  // YOU SHALL NOT SLEEP WITH THE LOCK HELD!
  sched(t);
  assert(holding_no_spin_lock());
}

// must called with the lock for the queue of tcb_node held
// 1. acquire the lock for the tcb
// 2. acquire the lock for the ready queue
// 3. release the lock for the ready queue
// 4. release the lock for the tcb
void do_unblock(list_node_t *tcb_node) {
  tcb_t *t = list_tcb(tcb_node);
  spin_lock_acquire(&t->lock);
  assert(t->status == TASK_BLOCKED || t->status == TASK_EXITED);
  // since killing is not often the case, all the resource recyclings are done in scheduler
  // here we can still free the tcb (because we are not on its kernel stack)
  // but it is unneccesary to check it here, we can safely move it to the ready queue
  if (t->status == TASK_BLOCKED)
    t->status = TASK_READY;
  log_line(PROC, "process %d (%s), thread %d is unblocked\n", t->pcb->pid,
           t->pcb->name, t->tid);
  LIST_REMOVE(tcb_node);
  ready_queue_insert(t);
  spin_lock_release(&t->lock);
}

void do_thread_create() {
  tcb_t *t = mythread();
  pcb_t *p = t->pcb;
  ptr_t thread_ptr = argraw(0);
  ptr_t thread_attr_ptr = argraw(1);
  ptr_t routine_ptr = argraw(2);
  ptr_t arg_ptr = argraw(3);
  spin_lock_acquire(&p->lock);
  tcb_t *nt = new_tcb(p, routine_ptr);
  if (nt == NULL) {
    t->trapframe->a0() = -1;
    return;
  }
  nt->status = TASK_READY;
  ready_queue_insert(nt);
  nt->trapframe->a0() = arg_ptr;
  spin_lock_release(&nt->lock);
  spin_lock_release(&p->lock);
  *(thread_t *)thread_ptr = nt - tcb;
  t->trapframe->a0() = 0;
  return;
}

void do_thread_yield(void) {
  tcb_t *t = mythread();
  do_yield();
  t->trapframe->a0() = 0;
}

void do_thread_exit() {
  tcb_t *t = mythread();
  ptr_t retval = argraw(0);
  if (free_tcb(t) != 0) {
    *(int *)retval = -1;
    return;
  }
  do_scheduler();
  if ((int *)retval != NULL)
    *(int *)retval = 0;
}

void do_exec(void) {
  tcb_t *t = mythread();
  char* prog = (char*)argraw(0);
  int argc;
  if (argint(1, &argc) == -1) {
    t->trapframe->a0() = -1;
    return ;
  }
  char** argv = (char**)argraw(2);
  pcb_t *np = new_pcb(prog, (ptr_t)load_task_by_name(prog));
  if (np == NULL) {
    t->trapframe->a0() = -1;
    return ;
  }
  tcb_t *nt = main_thread(np); 
  t->trapframe->a0() = np->pid;
  nt->trapframe->a0() = argc;
  nt->trapframe->sp() -= (argc + 1) * sizeof(ptr_t);
  uint64_t sp = nt->trapframe->sp();
  char** argv_ptr = (char**)sp;
  for (int i = 0; i < argc; i++) {
    // put argv on to the stack
    int len = strlen(argv[i]);
    nt->trapframe->sp() -= len + 1;
    strcpy((char*)sp, argv[i]);
    argv_ptr[i] = (char*)sp;
  }
  argv_ptr[argc] = NULL;
  nt->trapframe->a1() = (uint64_t)argv_ptr;
  // alignment: align the stack to 16 byte, padding with 0
  while((sp & 15) != 0) {
    sp--;
    *(char*)(sp) = 0;
  }
  nt->trapframe->sp() = sp;
}

void do_exit(void) {
  pcb_t* p = myproc();  
  spin_lock_acquire(&p->lock);
  kill_pcb(p);
  spin_lock_release(&p->lock);
}

void do_kill(void) {
  pid_t pid = argraw(0);
  pcb_t* p = get_pcb(pid);
  spin_lock_acquire(&p->lock);
  kill_pcb(p);
  spin_lock_release(&p->lock);
}

void do_waitpid(void) {
  tcb_t* t = mythread();
  int pid = argraw(0);
  pcb_t* p = pcb + pid - 1;
  spin_lock_acquire(&p->lock);
  if (p->status == PROC_INACTIVATE) {
    spin_lock_release(&p->lock);
    return ;
  }
  spin_lock_release(&p->lock);
  spin_lock_acquire(&p->wait_lock);
  do_block(&t->list, &p->wait_queue, &p->wait_lock);
}

void do_process_show() {
  for (int i = 0; i < NUM_MAX_TASK; i++) {
    spin_lock_acquire(&pcb[i].lock);
    if (pcb[i].status == PROC_INACTIVATE) {
      spin_lock_release(&pcb[i].lock);
      continue;
    }
    spin_lock_release(&pcb[i].lock);
    printk("pid: %d, name: %s\n", pcb[i].pid, pcb[i].name);
  }
}

void do_getpid() {
  tcb_t* t = mythread();
  t->trapframe->a0() = t->pcb->pid;
}