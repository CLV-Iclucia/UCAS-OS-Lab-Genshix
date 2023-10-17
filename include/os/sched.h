/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *        Process scheduling related content, such as: scheduler, process blocking,
 *                 process wakeup, process creation, process kill, etc.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <type.h>
#include <os/list.h>
#include <os/lock.h>

#define NUM_MAX_TASK 16
#define NAME_MAXLEN 16

/* used to save register infomation */
typedef struct regs_context
{
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
typedef struct switchto_context
{
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

static inline const char* task_status_str(task_status_t status) {
    switch (status) {
        case TASK_BLOCKED: return "BLOCKED";
        case TASK_RUNNING: return "RUNNING";
        case TASK_READY: return "READY";
        case TASK_EXITED: return "EXITED";
        default: return "UNKNOWN";
    }
}
/* Process Control Block */
typedef struct pcb
{

    regs_context_t* trapframe;
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;
    reg_t user_sp;
    /* previous, next pointer */
    list_node_t list;

    /* process id */
    pid_t pid;

    /* BLOCK | READY | RUNNING */
    task_status_t status;

    /* cursor position */
    int cursor_x;
    int cursor_y;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;
    switchto_context_t* ctx;

    char name[NAME_MAXLEN];   
    uint64_t strace_bitmask;  // what syscalls to trace?
    uint64_t lock_bitmask;   // locks held by this process
} pcb_t;

#define offsetof(type, member) ((size_t)(&((type*)0)->member))
#define HOLD_LOCK(proc, idx) ((proc)->lock_bitmask & LOCK_MASK(idx))
// given a pointer to a list_node_t, get the pcb_t that contains it
static inline pcb_t* list_pcb(list_node_t* ptr) {
    return (pcb_t*)((char*)ptr - offsetof(pcb_t, list));
}

/* ready queue to run */
extern list_head ready_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

/* current running task PCB */
extern pcb_t * volatile current_running;
extern pcb_t * volatile next_running;
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];
extern pcb_t sched_pcb;
extern const ptr_t sched_stack;

extern void switch_to(switchto_context_t *prev, switchto_context_t *next);
// a inline func myproc to get the current_running
static inline pcb_t* volatile myproc() 
{
    return current_running;
}
void init_pcb_pool();
void do_scheduler(void);
void do_sleep(void);
void do_yield(void);
void do_block(list_node_t *, list_head *queue);
void do_unblock(list_node_t *);
pcb_t* alloc_pcb();
pcb_t* new_pcb(const char* name);
void insert_pcb(list_head* queue, pcb_t* pcb);
void dump_all_proc();
void dump_context(switchto_context_t* ctx);
void dump_pcb(pcb_t* p);
/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

#endif
