#include <assert.h>
#include <debugs.h>
#include <os/csr.h>
#include <os/list.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/time.h>
#include <pgtable.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>
#include <type.h>

extern void user_trap_ret();

pcb_t pcb[NUM_MAX_TASK];
tcb_t tcb[NUM_MAX_THREADS];
switchto_context_t swtch_ctx[NUM_MAX_THREADS];
switchto_context_t sched_ctx[NR_CPUS];

tcb_t sched_tcb[NR_CPUS];
pcb_t sched_pcb[NR_CPUS];

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
  for (int i = 0; i < NUM_MAX_TASK; i++) pcb_pool_queue[i] = i;
  pcb_pool_lock.status = UNLOCKED;
  for (int i = 0; i < NUM_MAX_THREADS; i++) tcb_pool_queue[i] = i;
  tcb_pool_lock.status = UNLOCKED;
}

// no need to hold lock, this can be done atomically
pcb_t *alloc_pcb() {
  spin_lock_acquire(&pcb_pool_lock);
  if (pcb_pool_queue_head == pcb_pool_queue_tail) return NULL;
  int pcb_offset = pcb_pool_queue[pcb_pool_queue_head];
  pcb_pool_queue_head = (pcb_pool_queue_head + 1) % NUM_MAX_TASK;
  spin_lock_release(&pcb_pool_lock);
  return pcb + pcb_offset;
}

// no need to hold lock, this can be done atomically
tcb_t *alloc_tcb() {
  spin_lock_acquire(&tcb_pool_lock);
  if (tcb_pool_queue_head == tcb_pool_queue_tail) return NULL;
  int tcb_offset = tcb_pool_queue[tcb_pool_queue_head];
  tcb_pool_queue_head = (tcb_pool_queue_head + 1) % NUM_MAX_THREADS;
  spin_lock_release(&tcb_pool_lock);
  return tcb + tcb_offset;
}

// no need to hold lock, this can be done atomically
static void push_tcb(tcb_t *t) {
  spin_lock_acquire(&tcb_pool_lock);
  tcb_pool_queue_tail = (tcb_pool_queue_tail + 1) % NUM_MAX_THREADS;
  tcb_pool_queue[tcb_pool_queue_tail] = t - tcb + 1;
  spin_lock_release(&tcb_pool_lock);
}

static void push_pcb(pcb_t *p) {
  spin_lock_acquire(&pcb_pool_lock);
  pcb_pool_queue_tail = (pcb_pool_queue_tail + 1) % NUM_MAX_TASK;
  pcb_pool_queue[pcb_pool_queue_tail] = p - pcb;
  spin_lock_release(&pcb_pool_lock);
}

static uint8_t alloc_tid(pcb_t *p) {
  assert(spin_lock_holding(&p->lock));
  for (int i = 0; i < NUM_MAX_THREADS_PER_TASK; i++) {
    if ((p->tid_mask & (1 << i)) == 0) {
      p->tid_mask &= ~(1 << i);
      return i;
    }
  }
  return -1;
}

// the assembly code for this is in entry.S
extern void _thread_exit();
// must be called with the lock of pcb held
// this function will return with the lock of the new tcb HELD!
// 1. alloc new tcb and acquire the lock of the new tcb
// 1. release the lock of the queue of pcb
// 2. acquire the lock of the ready queue
// 3. release the lock of the ready queue
// note: this will not put the tcb to running, and will not set the args!
// the mapping of entry_pa is handled by pcb
tcb_t *new_tcb(pcb_t *p, uva_t entry_uva) {
  assert(spin_lock_holding(&p->lock));
  uint8_t new_tid = alloc_tid(p);
  if (new_tid == (uint8_t)(-1)) return NULL;
  tcb_t *t = alloc_tcb();
  if (t == NULL) return NULL;
  spin_lock_init(&t->lock);
  spin_lock_acquire(&(t->lock));
  spin_lock_init(&t->join_lock);
  t->ctx = swtch_ctx + (t - tcb);
  t->lock_bitmask = 0;
  t->wakeup_time = 0;
  t->kstack = kmalloc();
  t->trapframe = KPTR(regs_context_t, kmalloc());
  t->tid = new_tid;
  p->num_threads++;
  p->tid_mask |= (1 << new_tid);
  t->ustack = kmalloc();
  uvmmap(ustack_btm_uva(t), kva2pa(t->ustack), p->pgdir, PTE_R | PTE_W | PTE_U);
  t->trapframe->kernel_sp = ADDR(t->kstack) + PAGE_SIZE;
  t->trapframe->sstatus = SR_SPIE;
  t->trapframe->sp() = ADDR(ustack_btm_uva(t)) + PAGE_SIZE;
  t->trapframe->sepc = ADDR(entry_uva);
  t->ctx->ra = (uint64_t)user_trap_ret;
  t->ctx->sp = ADDR(t->kstack) + PAGE_SIZE;
  t->pcb = p;
  t->cpuid = -1;
  t->list.next = t->list.prev = &(t->list);
  t->thread_list.next = t->thread_list.prev = &(t->thread_list);
  t->join_queue.next = t->join_queue.prev = &(t->join_queue);
  LIST_INSERT_TAIL(&(p->threads), &(t->thread_list));
  return t;
}

// no need to hold lock, this can be done atomically
// but will exit with the lock of the pcb HELD!
// entry must be loaded
pcb_t *new_pcb(const char *name) {
  pcb_t *p = alloc_pcb();
  if (p == NULL) {
    printl("Error: PCB is full!\n");
    return NULL;
  }
  pa_t load_pa = load_task_by_name(name, 0);
  p->size = get_task_memsz(name);
  p->filesz = get_task_filesz(name);
  spin_lock_init(&(p->lock));
  spin_lock_init(&(p->wait_lock));
  spin_lock_acquire(&p->lock);
  p->pgdir = new_pgdir();
  // usually speaking, the mapping kva(starting with 0xffffffc) is not
  // overlapped with the uva
  share_pgtable(KVA(p->pgdir), KVA(PGDIR_KVA));
  p->pid = p - pcb + 1;
  p->status = PROC_ACTIVATE;
  strcpy(p->name, name);
  p->num_threads = p->tid_mask = 0;
  p->threads.next = p->threads.prev = &(p->threads);
  p->wait_queue.next = p->wait_queue.prev = &(p->wait_queue);
  p->parent = myproc();
  p->taskset = p->parent->taskset;
  uvmmap(UVA(USER_ENTRYPOINT_UVA), load_pa, p->pgdir,
         PTE_R | PTE_W | PTE_X | PTE_U);
  tcb_t *nt = new_tcb(p, UVA(USER_ENTRYPOINT_UVA));
  if (nt == NULL) return NULL;
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
  assert(spin_lock_holding(&p->lock) && spin_lock_holding(&t->lock));
  assert(t->current_queue == NULL);
  assert_msg(t != &sched_tcb[mycpuid()], "freeing the scheduler thread.");
  LIST_REMOVE(&(t->thread_list));
  p->num_threads--;
  p->tid_mask &= ~(1 << t->tid);
  // vmfree(p->pgdir, ADDR(ustack_uva(t)));
  uvmunmap(p->pgdir, ustack_btm_uva(t));
  kfree(t->kstack);
  kfree(KVA(t->trapframe));
  spin_lock_acquire(&t->join_lock);
  while (!LIST_EMPTY(t->join_queue)) {
    tcb_t *wt = list_thread(LIST_FIRST(t->join_queue));
    // if the thread is already exited, the a0 of the trapframe should store the
    // return value
    wt->trapframe->a0() = t->trapframe->a0();
    do_unblock(LIST_FIRST(t->join_queue));
  }
  spin_lock_release(&t->join_lock);
  spin_lock_release(&t->lock);
  // it is fine here, since at this time the status is already TASK_EXITED
  push_tcb(t);
  return 0;
}

// must be called with the lock of p held
int kill_pcb(pcb_t *p) {
  assert(spin_lock_holding(&p->lock));
  // run through all the threads
  list_node_t *node = p->threads.next;
  p->status = PROC_INACTIVATE;
  while (node != &(p->threads)) {
    tcb_t *t = list_thread(node);
    spin_lock_acquire(&t->lock);
    t->status = TASK_EXITED;
    spin_lock_release(&t->lock);
    node = node->next;
  }
  spin_lock_acquire(&p->wait_lock);
  if (LIST_EMPTY(p->wait_queue)) {
    spin_lock_release(&p->wait_lock);
    return 0;
  }
  while (!LIST_EMPTY(p->wait_queue)) do_unblock(LIST_FIRST(p->wait_queue));
  spin_lock_release(&p->wait_lock);
  return 0;
}

int kill_tcb(tcb_t *t) {
  assert(spin_lock_holding(&t->lock));
  t->status = TASK_EXITED;
  return 0;
}

void free_pcb(pcb_t *p) {
  assert(spin_lock_holding(&p->lock));
  assert(LIST_EMPTY(p->threads) && p->num_threads == 0);
  for (uint64_t uva = USER_ENTRYPOINT_UVA; uva < USER_ENTRYPOINT_UVA + p->size;
       uva += PAGE_SIZE)
    uvmunmap(p->pgdir, UVA(uva));
  uvmclear(p->pgdir, 2);
  spin_lock_release(&p->lock);
  push_pcb(p);
}

// hold no locks when entering this loop
// in this function, we also free exited tcb and pcb (if all of its tcb have
// exited) because the scheduler run on another kernel stack we can free the
// kernel stack of tcb safely
void do_scheduler(void) {
  while (1) {
    assert(holding_no_spin_lock());
    cpu_t *c = mycpu();
    check_sleeping();
    tcb_t *t = ready_queue_pop();
    if (t == NULL) continue;
    pcb_t *p = t->pcb;
    spin_lock_acquire(&p->lock);
    if ((p->taskset & (1 << (c->id))) == 0) {
      spin_lock_release(&p->lock);
      ready_queue_insert(t);
      continue;
    }
    spin_lock_acquire(&t->lock);
    if (t->status == TASK_EXITED) {
      free_tcb(t);
      if (p->num_threads == 0)
        free_pcb(p);
      else
        spin_lock_release(&p->lock);
      continue;
    }
    t->status = TASK_RUNNING;
    t->cpuid = c->id;
    c->current_running = t;
    spin_lock_release(&t->lock);
    spin_lock_release(&p->lock);
    c->timer_needs_reset = true;
    switch_pgdir(p->pid, kva2pa(KVA(p->pgdir)));
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
  if (t->pcb->pid == 0) panic("scheduler is yielding!\n");
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
  if (argint(0, (int *)&sleep_time) < 0) return;
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
  if (queue_lock != NULL) spin_lock_release(queue_lock);
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
  // since killing is not often the case, all the resource recyclings are done
  // in scheduler here we can still free the tcb (because we are not on its
  // kernel stack) but it is unneccesary to check it here, we can safely move it
  // to the ready queue
  if (t->status == TASK_BLOCKED) t->status = TASK_READY;
  log_line(PROC, "process %d (%s), thread %d is unblocked\n", t->pcb->pid,
           t->pcb->name, t->tid);
  LIST_REMOVE(tcb_node);
  ready_queue_insert(t);
  spin_lock_release(&t->lock);
}

static pa_t handle_uva_args(pcb_t *p, uva_t uva) {
  spin_lock_acquire(&p->lock);
  pa_t pa = uvmwalk(uva, p->pgdir, 0);
  spin_lock_release(&p->lock);
  if (ADDR(pa) == 0) handle_pgfault(NULL, ADDR(uva), 0);
  return pa;
}

void do_thread_create() {
  tcb_t *t = mythread();
  pcb_t *p = t->pcb;
  uva_t thread_uva = UVA(argraw(0));
  uva_t thread_attr_uva = UVA(argraw(1));
  uva_t routine_uva = UVA(argraw(2));
  uva_t arg_uva = UVA(argraw(3));
  pa_t thread_pa = PA(NULL), thread_attr_pa = PA(NULL), routine_pa = PA(NULL),
       arg_pa = PA(NULL);
  thread_pa = handle_uva_args(p, thread_uva);
  if (ADDR(thread_attr_uva))
    thread_attr_pa = handle_uva_args(p, thread_attr_uva);
  routine_pa = handle_uva_args(p, routine_uva);
  if (ADDR(arg_uva)) arg_pa = handle_uva_args(p, arg_uva);
  spin_lock_acquire(&p->lock);
  tcb_t *nt = new_tcb(p, routine_uva);
  if (nt == NULL) {
    spin_lock_release(&p->lock);
    t->trapframe->a0() = -1;
    return;
  }
  // when the thread returns, we make the thread jump to 0 and trigger a page
  // fault
  nt->trapframe->ra() = 0;
  prepare_sched(nt);
  nt->trapframe->a0() = ADDR(arg_uva);
  spin_lock_release(&nt->lock);
  spin_lock_release(&p->lock);
  *UPTR(thread_t, thread_uva) = nt - tcb;
  t->trapframe->a0() = 0;
  return;
}

void do_thread_yield(void) {
  tcb_t *t = mythread();
  do_yield();
  t->trapframe->a0() = 0;
}

void do_thread_exit(void) {
  tcb_t *t = mythread();
  spin_lock_acquire(&t->lock);
  kill_tcb(t);
  sched(t);
}

void do_thread_join() {
  tcb_t *t = mythread();
  thread_t tid;
  if (argint(0, &tid) < 0) {
    t->trapframe->a0() = -1;
    return;
  }
  tcb_t *wt = tcb + tid;
  spin_lock_acquire(&wt->lock);
  if (wt->status == TASK_EXITED) {
    spin_lock_release(&wt->lock);
    t->trapframe->a0() = 0;
    return;
  }
  spin_lock_release(&wt->lock);
  spin_lock_acquire(&t->lock);
  spin_lock_acquire(&wt->join_lock);
  do_block(&t->list, &wt->join_queue, &wt->join_lock);
}

static void prepare_ustack(tcb_t *t, int argc, char **argv) {
  assert(is_kva(t->trapframe));
  assert(t->trapframe->sp() ==
         ADDR(ustack_top_uva(t)) - (argc + 1) * sizeof(ptr_t));
  uint64_t psp = ADDR(t->ustack) + PAGE_SIZE - (argc + 1) * sizeof(ptr_t);
  uint64_t usp = t->trapframe->sp();
  assert(psp != 0);
  char **argv_ptr = (char **)psp;
  for (int i = 0; i < argc; i++) {
    // put argv on to the stack
    int len = strlen(argv[i]);
    psp -= len + 1;
    usp -= len + 1;
    strcpy((char *)psp, argv[i]);
    argv_ptr[i] = (char *)usp;
  }
  argv_ptr[argc] = NULL;
  // alignment: align the stack to 16 byte, padding with 0
  while ((psp & 15) != 0) {
    psp--;
    usp--;
    *(char *)(psp) = 0;
  }
  t->trapframe->sp() = usp;
}

void do_exec(void) {
  tcb_t *t = mythread();
  pcb_t *p = t->pcb;
  char *prog = (char *)argraw(0);
  int argc;
  if (argint(1, &argc) == -1) {
    t->trapframe->a0() = -1;
    return;
  }
  char **argv = (char **)argraw(2);
  if (!is_valid_uva(UVA(prog), p) || !is_valid_uva(UVA(argv), p)) {
    t->trapframe->a0() = -1;
    return;
  }
  pcb_t *np = new_pcb(prog);
  if (np == NULL) {
    t->trapframe->a0() = -1;
    return;
  }
  tcb_t *nt = main_thread(np);
  spin_lock_release(&np->lock);
  assert(ADDR(ustack_top_uva(nt)) == nt->trapframe->sp());
  t->trapframe->a0() = np->pid;
  nt->trapframe->a0() = argc;
  nt->trapframe->sp() -= (argc + 1) * sizeof(ptr_t);
  nt->trapframe->a1() = nt->trapframe->sp();
  prepare_ustack(nt, argc, (char **)argv);
  spin_lock_acquire(&nt->lock);
  prepare_sched(nt);
  spin_lock_release(&nt->lock);
}

void do_exit(void) {
  pcb_t *p = myproc();
  spin_lock_acquire(&p->lock);
  kill_pcb(p);
  spin_lock_release(&p->lock);
  tcb_t *t = mythread();
  spin_lock_acquire(&t->lock);
  sched(t);
}

void do_kill(void) {
  pid_t pid = argraw(0);
  if (pid >= NUM_MAX_TASK || pid < 0) return;
  pcb_t *p = get_pcb(pid);
  spin_lock_acquire(&p->lock);
  kill_pcb(p);
  spin_lock_release(&p->lock);
}

void do_waitpid(void) {
  tcb_t *t = mythread();
  int pid = argraw(0);
  pcb_t *p = pcb + pid - 1;
  spin_lock_acquire(&p->lock);
  if (p->status == PROC_INACTIVATE) {
    spin_lock_release(&p->lock);
    return;
  }
  spin_lock_release(&p->lock);
  spin_lock_acquire(&t->lock);
  spin_lock_acquire(&p->wait_lock);
  do_block(&t->list, &p->wait_queue, &p->wait_lock);
}

void do_ps() {
  for (int i = 0; i < NUM_MAX_TASK; i++) {
    spin_lock_acquire(&pcb[i].lock);
    if (pcb[i].status == PROC_INACTIVATE) {
      spin_lock_release(&pcb[i].lock);
      continue;
    }
    assert(pcb[i].num_threads > 0);
    uint8_t cpuid = main_thread(&pcb[i])->cpuid;
    spin_lock_release(&pcb[i].lock);
    if (cpuid == (uint8_t)-1)
      printk("pid: %d, name: %s, not running\n", pcb[i].pid, pcb[i].name);
    else
      printk("pid: %d, name: %s, running on cpu %d\n", pcb[i].pid, pcb[i].name,
             cpuid);
  }
}

void do_getpid() {
  tcb_t *t = mythread();
  t->trapframe->a0() = t->pcb->pid;
}

void do_taskset() {
  int pid;
  uint32_t mask;
  if (argint(0, &pid) < 0) return;
  if (argint(1, (int *)&mask) < 0) return;
  pcb_t *p = get_pcb(pid);
  spin_lock_acquire(&p->lock);
  if (p->status == PROC_INACTIVATE) {
    spin_lock_release(&p->lock);
    return;
  }
  p->taskset = mask;
  spin_lock_release(&p->lock);
  return;
}

tcb_t *copy_tcb(pcb_t *np, tcb_t *t) {
  assert(spin_lock_holding(&np->lock));
  tcb_t *nt = alloc_tcb();
  if (nt == NULL) return NULL;
  spin_lock_init(&nt->lock);
  spin_lock_acquire(&nt->lock);
  nt->ctx = swtch_ctx + (nt - tcb);
  nt->lock_bitmask = 0;
  nt->wakeup_time = t->wakeup_time;
  nt->kstack = kmalloc();
  nt->cursor_x = t->cursor_x;
  nt->cursor_y = t->cursor_y;
  nt->trapframe = KPTR(regs_context_t, kmalloc());
  *(nt->trapframe) = *(t->trapframe);
  nt->trapframe->kernel_sp = ADDR(nt->kstack) + PAGE_SIZE;
  nt->ctx->ra = (uint64_t)user_trap_ret;
  nt->ctx->sp = ADDR(nt->kstack) + PAGE_SIZE;
  nt->ustack = t->ustack;
  nt->tid = t->tid;
  nt->pcb = np;
  nt->cpuid = -1;
  nt->list.next = nt->list.prev = &(nt->list);
  nt->thread_list.next = nt->thread_list.prev = &(nt->thread_list);
  nt->join_queue.next = nt->join_queue.prev = &(nt->join_queue);
  nt->current_queue = NULL;
  LIST_INSERT_TAIL(&(np->threads), &(nt->thread_list));
  return nt;
}

pcb_t *copy_pcb(pcb_t *p, tcb_t *t) {
  pcb_t *np = alloc_pcb();
  if (np == NULL) return NULL;
  spin_lock_init(&np->lock);
  spin_lock_init(&np->wait_lock);
  spin_lock_acquire(&np->lock);
  np->pid = np - pcb + 1;
  np->threads.next = np->threads.prev = &(np->threads);
  np->wait_queue.next = np->wait_queue.prev = &(np->wait_queue);
  tcb_t *nt = copy_tcb(np, t);
  if (nt == NULL) goto bad;
  np->pgdir = new_pgdir();
  if (np->pgdir == NULL) goto bad;
  np->filesz = p->filesz;
  np->size = p->size;
  copy_pgdir(np->pgdir, p->pgdir, UVA(USER_ENTRYPOINT_UVA),
             UVA(USER_ENTRYPOINT_UVA + PGROUNDUP(p->size)), 2);
  assert(copy_page(np->pgdir, p->pgdir, ustack_btm_uva(t)) == 0);
  share_pgtable(KVA(np->pgdir), KVA(PGDIR_KVA));
  np->status = PROC_ACTIVATE;
  strcpy(np->name, p->name);
  np->num_threads = 1;
  np->tid_mask = (1 << (t->tid));
  np->parent = p;
  np->taskset = p->taskset;
  return np;

bad:
  spin_lock_release(&np->lock);
  return NULL;
}

void do_fork() {
  tcb_t *t = mythread();
  pcb_t *p = t->pcb;
  spin_lock_acquire(&p->lock);
  pcb_t *np = copy_pcb(p, t);
  if (np == NULL) {
    spin_lock_release(&p->lock);
    t->trapframe->a0() = -1;
    return;
  }
  spin_lock_release(&p->lock);
  tcb_t *nt = main_thread(np);
  t->trapframe->a0() = np->pid;
  nt->trapframe->a0() = 0;
  prepare_sched(nt);
  spin_lock_release(&nt->lock);
  spin_lock_release(&np->lock);
  assert(holding_no_spin_lock());
}