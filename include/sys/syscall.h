/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                       System call related processing
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

#ifndef INCLUDE_SYSCALL_H_
#define INCLUDE_SYSCALL_H_

#include <os/sched.h>
#include <type.h>
#include <asm/unistd.h>
#define syscall_transfer_i_v(do_fn, fn)\
void do_fn(void)\
{\
    pcb_t* p = myproc();\
    p->trapframe->a0() = fn();\
}

#define syscall_transfer_v_v(do_fn, fn)\
void do_fn(void)\
{\
    pcb_t* p = myproc();\
    fn();\
    p->trapframe->a0() = 0;\
}

#define syscall_transfer_i_i(do_fn, fn)\
void do_fn(void)\
{\
    pcb_t* p = myproc();\
    int arg0;\
    if (argint(0, &arg0) < 0) {\
        p->trapframe->a0() = -1;\
        return;\
    }\
    p->trapframe->a0() = fn(arg0);\
}

#define syscall_transfer_v_i(do_fn, fn)\
void do_fn(void)\
{\
    pcb_t* p = myproc();\
    int arg0;\
    if (argint(0, &arg0) < 0) {\
        p->trapframe->a0() = -1;\
        return;\
    }\
    fn(arg0);\
    p->trapframe->a0() = 0;\
}

#define syscall_transfer_v_p(do_fn, fn)\
void do_fn(void)\
{\
    pcb_t* p = myproc();\
    uint64_t arg0 = argraw(0);\
    fn((void*)arg0);\
    p->trapframe->a0() = 0;\
}

#define NUM_SYSCALLS 96

extern uint64_t argraw(int n);
extern int argint(int n, int* ip);
/* syscall function pointer */
extern long (*syscall[NUM_SYSCALLS])();
extern char* syscall_name[NUM_SYSCALLS];
extern void do_strace(void);
extern void handle_syscall(regs_context_t *regs, uint64_t stval, uint64_t scause);
extern void dump_trapframe(regs_context_t *regs);

#endif
