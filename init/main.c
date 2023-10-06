
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define version_buf 50
#define META_OFFSET_ADDR 0x502001f4
int version = 2; // version must between 0 and 9
char buf[version_buf];
#define TASK_MAXNUM 16
#define MAX_STRLEN 128
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

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
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
static void init_task_info(void)
{
    bios_putstr("initing tasks...\n\r");
    // todo: [p1-task4] init 'tasks' array via reading app-info sector
    // note: you need to get some related arguments from bootblock first
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

static inline char dtoc(int i) {
    return i + '0';
}

/* ChatGPT told me to jump to the user tasks like this */
typedef void (*EntryPoint)();

/************************************************************/
/* do not touch this comment. reserved for future projects. */
/************************************************************/
int main(void)
{
    // check whether .bss section is set to zero
    int check = bss_check();
    if (!check)
      panic("bss check failed!\n\r");
    // init jump table provided by kernel and bios(φωφ)
    init_jmptab();
    // init task information (〃'▽'〃)
    init_task_info();

    // output 'hello os!', bss check result and os version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }
    bios_putstr("Hello Genshix!\n\r");
    bios_putstr(buf);
    char input_buf[MAX_STRLEN];
    // todo: load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    bios_putstr(logo);
    bios_putstr("Available user tasks:\n");
    for (int i = 0; i < tasknum; i++) {
        img_putstr(tasks[i].name_offset);
        bios_putstr("\n");
    }
    bios_putstr("\033[?25h");
    while (1) {
        bios_putstr("> ");
        int ptr = 0;
        char c = getchar();
        while (c != '\r') {
            input_buf[ptr++] = c;
            c = getchar();
        }
        input_buf[ptr] = '\0';
        EntryPoint entry_point = (EntryPoint)load_task_by_name(input_buf);
        if (entry_point == 0) {
            bios_putstr(input_buf);
            bios_putstr(": task not found\n\r");
            continue;
        }
        entry_point();
    }
    // infinite while loop, where cpu stays in a low-power state (qaqqqqqqqqqqq)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
