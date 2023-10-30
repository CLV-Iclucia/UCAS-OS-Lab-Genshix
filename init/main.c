#include <debugs.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>
#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <os/csr.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <csr.h>
#include <screen.h>

#define version_buf 50
#define META_OFFSET_ADDR 0x502001f4
int version = 2; // version must between 0 and 9
char buf[version_buf];
char welcome[] = 
"========================================\n"
"               Genshix\n"
"\n"
"Welcome to the world of Genshix!\n"
"========================================\n";
#define TASK_MAXNUM 16
uint32_t tasknum;
// task info array
task_info_t tasks[TASK_MAXNUM];
char* syscall_name[NUM_SYSCALLS];
static int bss_check(void)
{
    for (int i = 0; i < version_buf; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (volatile long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (volatile long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (volatile long (*)())port_read_ch;
    jmptab[SD_READ]         = (volatile long (*)())sd_read;
    jmptab[SD_WRITE]        = (volatile long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (volatile long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (volatile long (*)())set_timer;
    jmptab[READ_FDT]        = (volatile long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (volatile long (*)())screen_move_cursor;
    jmptab[PRINT]           = (volatile long (*)())printk;
    jmptab[YIELD]           = (volatile long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (volatile long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (volatile long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (volatile long (*)())do_mutex_lock_release;
    jmptab[REFLUSH]         = (volatile long (*)())screen_reflush;
}
__attribute__((noreturn))
void init_panic(const char *msg)
{
    bios_putstr("panic: ");
    bios_putstr(msg);
    while (1)
    {
        asm volatile("wfi");
    }
}

#define NBUFFER_SECTORS 2
#define SEC_SIZE 512
#define OFFSET2IDX(offset) ((offset) >> 9)
char sd_buf[NBUFFER_SECTORS][SEC_SIZE];
int buf_sec_no[NBUFFER_SECTORS] = {-1, -1};

/* randomly return 0 or 1 for replacement */
static int random() 
{
    static int gen = 0x114514;
    gen ^= gen << 3;
    gen ^= gen >> 5;
    gen ^= gen << 7;
    return gen & 1;
}

char getc_img(uint32_t offset) 
{
    int sec_no = OFFSET2IDX(offset);
    if (sec_no < 0) panic("negative sector number\n\r");
    if (buf_sec_no[0] == sec_no)
        return sd_buf[0][offset & (SEC_SIZE - 1)];
    if (buf_sec_no[1] == sec_no)
        return sd_buf[1][offset & (SEC_SIZE - 1)];
    int evict = random();
    buf_sec_no[evict] = sec_no;
    bios_sd_read((uint32_t)(sd_buf[evict]), 1, sec_no);
    return sd_buf[evict][offset & (SEC_SIZE - 1)];
}

static inline uint32_t readint_img(uint32_t offset) 
{
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
static void img_putstr(uint32_t offset) 
{
    char c = getc_img(offset);
    while(c != '\0') {
        bios_putchar(c);
        c = getc_img(++offset);
    }
}

static char getchar() 
{
    while (1) {
        int c = bios_getchar();
        if (c != -1) { 
            if (c >= 256) panic("Invalid getchar input encountered!\n\r");
            bios_putchar(c);
            if (c == '\r') bios_putchar('\n');
     //       if (c == '\b') {
      //          bios_putchar(' ');
       //         bios_putstr("\033[1C");
        //    }
            return (char)c;
        }
    }
}

/************************************************************/

static char* init_tasks[] = 
{
    "print2",
    "print1",
    "fly",
    "lock1",
    "lock2",
    "sleep",
    "timer",
    "threads",
    "",
};

static void init_pcb(void)
{
    current_running = &sched_tcb;
    current_running->trapframe = (regs_context_t*)allocKernelPage(1);
    current_running->trapframe->kernel_sp = current_running->kernel_sp + PAGE_SIZE;
    for (int i = 0; i < tasknum; i++) {
        // load the task into the memory
        if (init_tasks[i][0] == '\0') break;
        printl("loading task %s\n", init_tasks[i]);
        uint32_t load_addr = (uint32_t)load_task_by_name(init_tasks[i]);
        pcb_t* p = new_pcb(init_tasks[i], load_addr);
        if (p == NULL) panic("new_pcb failed!\n\r");
    }
    next_running = list_tcb(ready_queue.next);
    log_block(PROC, dump_all_threads());
}

#define register_syscall(id, name)\
do {\
syscall[id] = (long (*)())do_##name;\
syscall_name[id] = #name;\
} while(0)

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    register_syscall(SYSCALL_YIELD, yield);
    register_syscall(SYSCALL_SLEEP, sleep);
    // syscall about screen
    register_syscall(SYSCALL_WRITE, write);
    register_syscall(SYSCALL_CURSOR, move_cursor);
    register_syscall(SYSCALL_REFLUSH, reflush);
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
    register_syscall(SYSCALL_GET_SCHED_TIMES, sched_times);
}

/************************************************************/
/* do not touch this comment. reserved for future projects. */
/************************************************************/

static void init_task_info(void)
{
    bios_putstr("initing tasks...\n\r");
    meta_offset = *(uint32_t*)(META_OFFSET_ADDR);
    uint32_t ptr = meta_offset;
    tasknum = readint_img(ptr);
    ptr += 4;
    if (tasknum < 0) init_panic("#tasks is negative!\n\r");
    if (tasknum > TASK_MAXNUM) init_panic("too many tasks!\n\r");
    if (tasknum == 0) {
        bios_putstr("warning: no available tasks!\n\r");
        return ;
    }
    for (int i = 0; i < tasknum; i++, ptr += 8) {
        tasks[i].offset = readint_img(ptr);
        tasks[i].name_offset = readint_img(ptr + 4);
    }
    name_region_offset = tasks[0].name_offset;
    if (ptr != name_region_offset) init_panic("user image corrupted!\n\r");
}

int main(void)
{
    int check = bss_check();
    if (!check)
      init_panic("bss check failed!\n\r");
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();
    // Init task information (〃'▽'〃)
    init_task_info();
    init_pool();
    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);
    set_timer(time_base);
    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");
    printk("%s", welcome);
    latency(2);
    screen_reflush();
    w_sscratch((uint64_t)(sched_tcb.trapframe));
    while (1)
    {
        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // Read CPU frequency (｡•ᴗ-)_
        enable_preempt();
        asm volatile("wfi"); 
    }

    return 0;
}
