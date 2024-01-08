#include <asm.h>
#include <asm/unistd.h>
#include <assert.h>
#include <common.h>
#include <csr.h>
#include <debugs.h>
#include <os/csr.h>
#include <os/io.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/list.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>
#include <type.h>

#define version_buf 50
#define META_OFFSET_KVA 0xffffffc0502001f4
int version = 2;  // version must between 0 and 9
char buf[version_buf];
char welcome[] =
    "========================================\n"
    "               Genshix\n"
    "\n"
    "Welcome to the world of Genshix!\n"
    "========================================\n";
#define TASK_MAXNUM 32
uint32_t tasknum;
// task info array
task_info_t tasks[TASK_MAXNUM];
char* syscall_name[NUM_SYSCALLS];
static int bss_check(void) {
  for (int i = 0; i < version_buf; ++i) {
    if (buf[i] != 0) {
      return 0;
    }
  }
  return 1;
}

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];

static void init_jmptab(void) {
  volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

  jmptab[CONSOLE_PUTSTR] = (volatile long (*)())port_write;
  jmptab[CONSOLE_PUTCHAR] = (volatile long (*)())port_write_ch;
  jmptab[CONSOLE_GETCHAR] = (volatile long (*)())port_read_ch;
  jmptab[SD_READ] = (volatile long (*)())sd_read;
  jmptab[SD_WRITE] = (volatile long (*)())sd_write;
  jmptab[QEMU_LOGGING] = (volatile long (*)())qemu_logging;
  jmptab[SET_TIMER] = (volatile long (*)())set_timer;
  jmptab[READ_FDT] = (volatile long (*)())read_fdt;
  jmptab[MOVE_CURSOR] = (volatile long (*)())screen_move_cursor;
  jmptab[PRINT] = (volatile long (*)())printk;
  jmptab[YIELD] = (volatile long (*)())do_scheduler;
  jmptab[MUTEX_INIT] = (volatile long (*)())do_mutex_lock_init;
  jmptab[MUTEX_ACQ] = (volatile long (*)())do_mutex_lock_acquire;
  jmptab[MUTEX_RELEASE] = (volatile long (*)())do_mutex_lock_release;
  jmptab[REFLUSH] = (volatile long (*)())screen_reflush;
}
__attribute__((noreturn)) void init_panic(const char* msg) {
  bios_putstr("panic: ");
  bios_putstr(msg);
  while (1) {
    asm volatile("wfi");
  }
}

#define NBUFFER_SECTORS 2
#define SEC_SIZE 512
#define OFFSET2IDX(offset) ((offset) >> 9)
char sd_buf[NBUFFER_SECTORS][SEC_SIZE];
int buf_sec_no[NBUFFER_SECTORS] = {-1, -1};

/* randomly return 0 or 1 for replacement */
static int random() {
  static int gen = 0x114514;
  gen ^= gen << 3;
  gen ^= gen >> 5;
  gen ^= gen << 7;
  return gen & 1;
}

char getc_img(uint32_t offset) {
  int sec_no = OFFSET2IDX(offset);
  if (sec_no < 0) panic("negative sector number\n\r");
  if (buf_sec_no[0] == sec_no) return sd_buf[0][offset & (SEC_SIZE - 1)];
  if (buf_sec_no[1] == sec_no) return sd_buf[1][offset & (SEC_SIZE - 1)];
  int evict = random();
  buf_sec_no[evict] = sec_no;
  bios_sd_read((uint32_t)(sd_buf[evict]), 1, sec_no);
  return sd_buf[evict][offset & (SEC_SIZE - 1)];
}

static inline uint32_t readint_img(uint32_t offset) {
  char c0, c1, c2, c3;
  c0 = getc_img(offset);
  c1 = getc_img(offset + 1);
  c2 = getc_img(offset + 2);
  c3 = getc_img(offset + 3);
  uint32_t lo = c0 | ((int)c1 << 8);
  uint32_t hi = ((int)c2 << 16) | ((int)c3 << 24);
  return lo | hi;
}

uint32_t meta_offset;
uint32_t name_region_offset;
static void img_putstr(uint32_t offset) {
  char c = getc_img(offset);
  while (c != '\0') {
    bios_putchar(c);
    c = getc_img(++offset);
  }
}

/************************************************************/

static char* init_tasks[] = {
  //  "shell",
    "fork",
    "",
};

static void init_pcb(void) {
  for (int i = 0; i < NUM_MAX_TASK; i++) {
    pcb[i].status = PROC_INACTIVATE;
    spin_lock_init(&pcb[i].lock);
    spin_lock_init(&pcb[i].wait_lock);
    pcb[i].wait_queue.next = pcb[i].wait_queue.prev = &pcb[i].wait_queue;
  }
  for (int i = 0; i < NUM_MAX_THREADS; i++) {
    tcb[i].status = TASK_EXITED;
    tcb[i].current_queue = NULL;
    spin_lock_init(&tcb[i].lock);
  }
  for (int i = 0; i < tasknum; i++) {
    // load the task into the memory
    if (init_tasks[i][0] == '\0') break;
    printl("loading task %s\n", init_tasks[i]);
    pcb_t *p = new_pcb(init_tasks[i]);
    if (p == NULL) panic("new_pcb failed!\n\r");
    p->taskset = ((2 << NR_CPUS) - 1);
    tcb_t* t = main_thread(p);
    spin_lock_acquire(&t->lock);
    prepare_sched(t);
    spin_lock_release(&t->lock);
    spin_lock_release(&p->lock);
  }
}

#define register_syscall(id, name)       \
  do {                                   \
    syscall[id] = (long (*)())do_##name; \
    syscall_name[id] = #name;            \
  } while (0)

static void init_syscall(void) {
  // TODO: [p2-task3] initialize system call table.
  register_syscall(SYSCALL_YIELD, yield);
  register_syscall(SYSCALL_SLEEP, sleep);
  register_syscall(SYSCALL_FORK, fork);
  // syscall about screen
  register_syscall(SYSCALL_WRITE, write);
  register_syscall(SYSCALL_CURSOR, move_cursor);
  register_syscall(SYSCALL_REFLUSH, reflush);
  register_syscall(SYSCALL_CLEAR_REGION, clear_region);
  register_syscall(SYSCALL_CURSOR_X, move_cursor_x);
  register_syscall(SYSCALL_CURSOR_Y, move_cursor_y);
  // syscall about lock
  register_syscall(SYSCALL_LOCK_INIT, mutex_init);
  register_syscall(SYSCALL_LOCK_ACQ, mutex_acquire);
  register_syscall(SYSCALL_LOCK_RELEASE, mutex_release);
  // syscall about time
  register_syscall(SYSCALL_GET_TIMEBASE, get_timebase);
  register_syscall(SYSCALL_GET_TICK, get_tick);
  register_syscall(SYSCALL_STRACE, strace);
  register_syscall(SYSCALL_THREAD_CREATE, thread_create);
  register_syscall(SYSCALL_THREAD_EXIT, thread_exit);
  register_syscall(SYSCALL_THREAD_YIELD, thread_yield);
  register_syscall(SYSCALL_THREAD_JOIN, thread_join);
  register_syscall(SYSCALL_GETPID, getpid);
  register_syscall(SYSCALL_KILL, kill);
  register_syscall(SYSCALL_PS, ps);
  register_syscall(SYSCALL_EXEC, exec);
  register_syscall(SYSCALL_EXIT, exit);
  register_syscall(SYSCALL_WAITPID, waitpid);
  register_syscall(SYSCALL_READCH, getchar);
  register_syscall(SYSCALL_TASKSET, taskset);
  register_syscall(SYSCALL_BARR_INIT, barr_init);
  register_syscall(SYSCALL_BARR_WAIT, barr_wait);
  register_syscall(SYSCALL_BARR_DESTROY, barr_destroy);
  register_syscall(SYSCALL_COND_INIT, cond_init);
  register_syscall(SYSCALL_COND_WAIT, cond_wait);
  register_syscall(SYSCALL_COND_SIGNAL, cond_signal);
  register_syscall(SYSCALL_COND_BROADCAST, cond_broadcast);
  register_syscall(SYSCALL_COND_DESTROY, cond_destroy);
  register_syscall(SYSCALL_SEMA_INIT, sema_init);
  register_syscall(SYSCALL_SEMA_UP, sema_up);
  register_syscall(SYSCALL_SEMA_DOWN, sema_down);
  register_syscall(SYSCALL_SEMA_DESTROY, sema_destroy);
  register_syscall(SYSCALL_MBOX_OPEN, mailbox_open);
  register_syscall(SYSCALL_MBOX_CLOSE, mailbox_close);
  register_syscall(SYSCALL_MBOX_SEND, mailbox_send);
  register_syscall(SYSCALL_MBOX_RECV, mailbox_recv);
}

static void init_sched_pcb() {
  for (int i = 0; i < NR_CPUS; i++) {
    sched_tcb[i].status = TASK_READY;
    sched_tcb[i].cpuid = i;
    sched_tcb[i].cursor_x = 0;
    sched_tcb[i].cursor_y = i;
    sched_tcb[i].kstack = kmalloc();
    sched_tcb[i].trapframe = KPTR(regs_context_t, kmalloc());
    sched_tcb[i].ctx = &sched_ctx[i];
    sched_tcb[i].status = TASK_RUNNING;
    sched_tcb[i].current_queue = NULL;
    sched_tcb[i].trapframe->kernel_sp = ADDR(sched_tcb[i].kstack) + PAGE_SIZE;
    sched_tcb[i].trapframe->sstatus = SR_SPIE;
    sched_tcb[i].trapframe->sepc = USER_ENTRYPOINT_UVA;
    sched_tcb[i].ctx->ra = (uint64_t)user_trap_ret;
    sched_tcb[i].ctx->sp = ADDR(sched_tcb[i].kstack) + PAGE_SIZE;
    sched_tcb[i].pcb = &sched_pcb[i];
    spin_lock_init(&sched_tcb[i].lock);
    spin_lock_init(&sched_pcb[i].lock);
    sched_pcb[i].status = PROC_ACTIVATE;
    sched_pcb[i].num_threads = sched_pcb[i].tid_mask = 1;
    sched_pcb[i].threads = (list_node_t){&sched_tcb[i].thread_list, &sched_tcb[i].thread_list};
  }
}

/************************************************************/
/* do not touch this comment. reserved for future projects. */
/************************************************************/

static void init_task_info(void) {
  bios_putstr("initing tasks...\n\r");
  meta_offset = *(uint32_t*)META_OFFSET_KVA;
  uint32_t ptr = meta_offset;
  tasknum = readint_img(ptr);
  ptr += 4;
  if (tasknum < 0) init_panic("#tasks is negative!\n\r");
  if (tasknum > TASK_MAXNUM) init_panic("too many tasks!\n\r");
  if (tasknum == 0) {
    bios_putstr("warning: no available tasks!\n\r");
    return;
  }
  for (int i = 0; i < tasknum; i++, ptr += sizeof(task_info_t)) {
    tasks[i].offset = readint_img(ptr);
    tasks[i].name_offset = readint_img(ptr + 4);
    tasks[i].memsz = readint_img(ptr + 8);
  }
  name_region_offset = tasks[0].name_offset;
  if (ptr != name_region_offset) init_panic("user image corrupted!\n\r");
}

static spin_lock_t temp_map_lock;
static bool temp_map_cleared = false;

extern void enable_kvm4bios(void);
int main(void) {
  if (mycpuid() == 0) {
    int check = bss_check();
    if (!check) init_panic("bss check failed!\n\r");
    enable_kvm4bios();
    init_memory();
    smp_init();
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();
    // Init task information (〃'▽'〃)
    init_sched_pcb();
    init_task_info();
    init_pool();
    // Init lock mechanism o(´^｀)o
    init_locks();
    init_conditions();
    init_semaphores();
    init_barriers();
    init_mbox();
    // Init interrupt (^_^)
    init_exception();
    // Init system call table (0_0)
     // TODO: [p5-task3] Init plic
    // plic_init(plic_addr, nr_irqs);
    // printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

    // Init network device
    e1000_init();

    init_syscall();
    // Init screen (QAQ)
    init_screen();
    // Read CPU frequency (｡•ᴗ-)_
    // assume the two harts have the same frequency
    time_base = bios_read_fdt(TIMEBASE);
    spin_lock_init(&temp_map_lock);
    wakeup_other_hart();
  } else {
    init_exception();
    kvm_unmap_early();
    spin_lock_acquire(&temp_map_lock);
    temp_map_cleared = true;
    spin_lock_release(&temp_map_lock);
  }
  // Init Process Control Blocks |•'-'•) ✧
  printk("CPU %d: Glory to mankind!\n", mycpuid());
  latency(2);
  if (mycpuid() == 0) {
    screen_clear();
    while (true) {
      spin_lock_acquire(&temp_map_lock);
      if (temp_map_cleared) {
        spin_lock_release(&temp_map_lock);
        break;
      }
      spin_lock_release(&temp_map_lock);
    }
    init_pcb();
  }
  set_timer(time_base);
  w_sscratch((uint64_t)(mythread()->trapframe));
  while (1) {
    // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
    // Read CPU frequency (｡•ᴗ-)_
    enable_preempt();
    asm volatile("wfi");
  }
  return 0;
}