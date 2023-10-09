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
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

#define version_buf 50
#define META_OFFSET_ADDR 0x502001f4
int version = 2; // version must between 0 and 9
char buf[version_buf];
#define TASK_MAXNUM 16
uint32_t tasknum;
// task info array
task_info_t tasks[TASK_MAXNUM];
char logo[] = 

"                                                                                                                                                     \n\r"
"                                                                                                                                                     \n\r"
"                                                                          =^                                                                         \n\r"
"                                                                          @@                                                                         \n\r"
"                                                                         ,@@^                                                                        \n\r"
"                                                                         @@@@                                                                        \n\r"
"                                                                        @@@@@@`                                                                      \n\r"
"                                                                    ,/@@@@@@@@@@\\`                                                                   \n\r"
"                                                                   ,\\@@@@@@@@@@@@/`                                                                  \n\r"
"                                                           ]/@@\\       \\@@@@@@/       /@@\\]                                                          \n\r"
"           ,/@@@@@@@\\`                                 ]@@@@@@@@^       ,@@@@`       =@@@@@@@@]                                                      \n\r"
"         /@@@@@@@@@@@@@\\                            /@@@@@@@@@@@@\\       =@@^       /@@@@@@@@@@@@\\                                              ,]@^ \n\r"
"       ,@@@@@@@@`    ,@@@@@\\                     ,@@@@@@@@@@@@@@@@@       @@       @@@@@@@@@@@@@@@@@`                                      ]@@@@@@   \n\r"
"      /@@@@@@@/        ,@@@@                    /@@@@@@@@@@@@@/[[`        =^         [[[@@@@@@@@@@@@@@                                =@@@@@@@@@@    \n\r"
"     /@@@@@@@/           @@@                   @@@@@@@@[`                                    ,[@@@@@@@@                                 =@@@@@@@     \n\r"
"    =@@@@@@@/            ,@@                    \\@@@^                  ]@@@@@]]]                 =@@@/                  ,]/@@`         =@@@@@@@      \n\r"
"   ,@@@@@@@@              \\@                      [@@@`              ,@@@   [@@@\\              ,/@@[                   [@@@@@@\\       =@@@@@@@       \n\r"
"   =@@@@@@@/              ,@ ,@\\]                                   =@@@      \\@@^                                       \\@@@@@@`    =@@@@@@@        \n\r"
"   @@@@@@@@^               [   O@@@@@@@@@@@@^ =@@@@\\      \\@@@@@@@`,@@@@       @@@ ,@@@@@@@/   =@@@@@@@^  \\@@@@@@@@@/     ,@@@@@@^  ,@@@@@@@`        \n\r"
"  =@@@@@@@@                    =@@@@@[[[[\\@@@  ,@@@@\\      =@@@@/  =@@@@\\       \\@^  @@@@@`     ,@@@@@      =@@@@@^         @@@@@@\\ @@@@@@@`         \n\r"
"  =@@@@@@@@                     @@@@^      \\@   @@@@@\\     *@@@@^  =@@@@@@\\          =@@@@       @@@@O       @@@@@           \\@@@@@@@@@@@@`          \n\r"
"  =@@@@@@@@                     @@@@^       \\`  @@@@@@\\    *@@@@^  ,@@@@@@@@`        =@@@O       @@@@O       @@@@@            =@@@@@@@@@@`           \n\r"
"  =@@@@@@@@                     @@@@^           @@@@@@@\\    @@@@^   \\@@@@@@@@@`      =@@@@       @@@@O       @@@@@             ,@@@@@@@@`            \n\r"
"  =@@@@@@@@     ,\\@@@@@@@@@@@@[ @@@@^ ,]@@@     @@@@O,@@\\   @@@@^    \\@@@@@@@@@@     =@@@@       @@@@O   /@@^@@@@@              /@@@@@@\\             \n\r"
"   @@@@@@@@         O@@@@@@@^   @@/]@@@@@@@`    @@@@O =@@\\ ,@@@@^     =@@@@@@@@@@`   =@@@@@@@@@@@@@@@O   /@@^@@@@@             /@@@@@@@@@            \n\r"
"   \\@@@@@@@^         @@@@@@@^   O\\@@@@@@@@@@\\   @@@@O  ,@@^*@@@@^       \\@@@@@@@@@`  =@@@@@@@@@@@@@@@O ,@@@@^@@@@@]`          /@@@@@@@@@@@`          \n\r"
"   ,@@@@@@@\\         O@@@@@@^  ,@@@@@@@@@/[[[`  @@@@O   =@@\\@@@@^         \\@@@@@@@O  =@@@@@OOOOO@@@@@O [[[[O^@@@@@O@\\        /@@@@@@/@@@@@@^         \n\r"
"    =@@@@@@@`        O@@@@@@^ ,@@@@@^           @@@@O    =@@@@@@^  ^        \\@@@@@@  =@@@@       O@@@O       @@@@@O@@\\      /@@@@@@/  \\@@@@@\\        \n\r"
"     \\@@@@@@O        O@@@@@@` @@@@@@^        =  O@@@^     =@@@@@^  @         =@@@@@  =@@@O       O@@@O       @@@@@  \\@\\    =@@@@@@@    =@@@@@@       \n\r"
"      \\@@@@@@\\       O@@@@@@`O` @@@@^       =@  O@@@^      =@@@@^  @O         =@@@O  =@@@O       O@@@O       @@@@O    \^  =@@@@@@@      ,@@@@@@`     \n\r"
"       \\@@@@@@O`     O@@@@@/   =@@@@\\      /@O  O@@@\\       =@@@^  OOO`       =@@@^  =@@@O       O@@@\\       O@@@O       /@@@@@@@        /@@@@@@\\    \n\r"
"         \\@@@@@@O`  ,@@@@/`    O@@@OO@@@@@OOO^ =OOOOO^       =OO^  OOOO\\     ,OOOO   OOOOO^     =OOOOO`    ,O@@O@OO    /O@@@@@@@@O`     O@@@@@@@@O\\  \n\r"
"           [O@OOOOOO@O/`     ,OO[[[`****       *              =OO  =OOOOOO]]/OOOO   =OOOO[[                           /O@@@@@@@@@@     /@@@@@@@@@@@\\  \n\r"
"                                                                \\^      [OOOO/[    =O/                                                               \n\r"
"                                                                                                                                                     \n\r";

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

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;
    jmptab[REFLUSH]         = (long (*)())screen_reflush;
}
__attribute__((noreturn))
void panic(const char *msg)
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
    if (buf_sec_no[0] == sec_no)
        return sd_buf[0][offset & (SEC_SIZE - 1)];
    if (buf_sec_no[1] == sec_no)
        return sd_buf[1][offset & (SEC_SIZE - 1)];
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
/* 
recap my format

------------                0
bootloader                  512b
------------
kernel                      unknown
------------                tasks[0].offset
app 1
------------                tasks[1].offset
app 2
------------
...
------------                meta_offset
#tasks                      4b
------------
tasks[0].offset          4b
tasks[0].name_offset     4b
------------
tasks[1].offset          4b
tasks[1].name_offset     4b
------------
...
------------                tasks[0].name_offset
tasks[0].name
------------                tasks[1].name_offset
tasks[1].name
------------
...
*/
static void img_putstr(uint32_t offset) {
    char c = getc_img(offset);
    while(c != '\0') {
        bios_putchar(c);
        c = getc_img(++offset);
    }
}

static char getchar() {
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
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

}

static char init_tasks[TASK_MAXNUM][NAME_MAXLEN] = {
    "print2",
    "print1",
    "fly",
    "lock1",
    "lock2",
};

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    // add all the init_tasks into the ready_queue
    // this process called "init" is also added to the ready_queue
    // for all the tasks in the init_tasks
    printl("initing tasks...\n");
    for (int i = 0; i < tasknum; i++) {
        // load the task into the memory
        if (init_tasks[i][0] == '\0') continue;
        printl("loading task %s\n", init_tasks[i]);
        uint32_t load_addr = (uint32_t)load_task_by_name(init_tasks[i]);
        pcb_t* p = new_pcb(init_tasks[i]);
        p->ctx->ra = load_addr;
        p->user_sp = p->ctx->sp = allocUserPage(1);
        p->status = TASK_READY;
        if (p == NULL) panic("new_pcb failed!\n\r");
        insert_pcb(&ready_queue, p);
        printl("task %s loaded\n", init_tasks[i]);
    }
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    insert_pcb(&ready_queue, &pid0_pcb);
    current_running = &pid0_pcb;
    if (current_running->list.next == &ready_queue)
        next_running = list_pcb(ready_queue.next);
    else
        next_running = list_pcb(current_running->list.next);
    dump_all_proc();
    printl("init tasks finished\n");
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/
static inline char dtoc(int i) {
    return i + '0';
}

typedef void (*EntryPoint)();

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
    if (tasknum < 0) panic("#tasks is negative!\n\r");
    if (tasknum > TASK_MAXNUM) panic("too many tasks!\n\r");
    if (tasknum == 0) {
        bios_putstr("warning: no available tasks!\n\r");
        return ;
    }
    for (int i = 0; i < tasknum; i++, ptr += 8) {
        tasks[i].offset = readint_img(ptr);
        tasks[i].name_offset = readint_img(ptr + 4);
    }
    name_region_offset = tasks[0].name_offset;
    if (ptr != name_region_offset) panic("user image corrupted!\n\r");
}

int main(void)
{
    int check = bss_check();
    if (!check)
      panic("bss check failed!\n\r");
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();
    // Init task information (〃'▽'〃)
    init_task_info();
    init_pcb_pool();
    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

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

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}
