#include "os/lock.h"
#include <debugs.h>
#include <os/sched.h>
#include <printk.h>
#include <sys/syscall.h>
#include <assert.h>
#define SYSCALL_BITMASK(sysno) (1ull << (sysno))
long (*syscall[NUM_SYSCALLS])();

static char *reg_name[] = {
    "zero ", " ra  ", " sp  ", " gp  ", " tp  ", " t0  ", " t1  ", " t2  ",
    "s0/fp", " s1  ", " a0  ", " a1  ", " a2  ", " a3  ", " a4  ", " a5  ",
    " a6  ", " a7  ", " s2  ", " s3  ", " s4  ", " s5  ", " s6  ", " s7  ",
    " s8  ", " s9  ", " s10 ", " s11 ", " t3  ", " t4  ", " t5  ", " t6  "};

void dump_trapframe(regs_context_t *regs) {
  // print the trapframe information of current process
  printl("trapframe in context %lx\n", regs);
  for (int i = 0; i < 32; i += 3) {
    for (int j = 0; j < 3 && i + j < 32; ++j) {
      printl("%s : %016lx ", reg_name[i + j], regs->regs[i + j]);
    }
    printl("\n\r");
  }
  printl("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r", regs->sstatus,
         regs->sbadaddr, regs->scause);
  printl("sepc: 0x%lx\n\r", regs->sepc);
}

uint64_t argraw(int n) {
  regs_context_t *r = mythread()->trapframe;
  assert(n >= 0 && n < 6);
  switch(n) {
    case 0: return r->a0();
    case 1: return r->a1();
    case 2: return r->a2();
    case 3: return r->a3();
    case 4: return r->a4();
    case 5: return r->a5();
  }
  panic("argraw: should not reach here");
}

int argint(int n, int *ip) {
  *ip = argraw(n);
  return 0;
}

void do_strace(void) {
  tcb_t *t = mythread();
  uint64_t strace_bitmask = argraw(0);
  t->strace_bitmask = strace_bitmask;
  t->trapframe->a0() = 0;
}

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause) 
{
  assert(holding_no_spin_lock());
  int sysno = regs->a7();
  log_line(SYSCALL, "handling syscall: %d\n", sysno);
  log_block(INTR, dump_trapframe(regs));
  tcb_t *t = mythread();
  t->trapframe->sepc += 4;
  syscall[sysno]();
  int ret = regs->a0();
  if (t->strace_bitmask & SYSCALL_BITMASK(sysno)) {
    if (syscall_name[sysno])
      printk("%s(pid: %d), thread %d: syscall %s -> %d\n", t->pcb->name, t->pcb->pid, t->tid,
             syscall_name[sysno], ret);
    else
      printk("%s(pid: %d), thread %d: unknown syscall %d\n", t->pcb->name, t->pcb->pid, t->tid, sysno);
  }
  assert(holding_no_spin_lock());
}
