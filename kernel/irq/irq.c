#include <os/lock.h>
#include <assert.h>
#include <common.h>
#include <debugs.h>
#include <os/csr.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/time.h>
#include <os/smp.h>
#include <printk.h>
#include <screen.h>
#include <stdbool.h>
handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

static inline bool is_interrupt(uint64_t scause) { return (scause >> 63) != 0; }

static inline uint64_t exception_code(uint64_t scause) { return scause & 0xfff; }

// note: some operations based on the assumption that we will trap ret in the same CPU need to be modified
void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause) 
{
  assert(holding_no_spin_lock());
  cpu_t *c = mycpu();
  c->timer_needs_reset = false;
  tcb_t* t = c->current_running;
  spin_lock_acquire(&t->lock);
  if (mythread()->status == TASK_EXITED) {
    // TODO: what should we do?
  }
  spin_lock_release(&t->lock);
  if (is_supervisor_mode()) {
    log(INTR, "Interrupt: scause: %lx, stval: %lx in supervisor mode", scause, stval);
    if (is_interrupt(scause)) {
      irq_table[exception_code(scause)](regs, stval, scause);
    } else {
      panic("An exception happens in kernel!\n");
    }
    return;
  }
  log(INTR, "Interrupt: scause: %lx, stval: %lx in user mode", scause, stval);
  set_supervisor_mode();
  w_stvec((uint64_t)kernel_exception_handler);
  if (is_interrupt(scause)) {
    irq_table[exception_code(scause)](regs, stval, scause);
  } else {
    exc_table[exception_code(scause)](regs, stval, scause);
  }
  assert(holding_no_spin_lock());
}

// do not hold any lock when entering this function!
void user_trap_ret() 
{
  assert(holding_no_spin_lock());
  bool needs_reset = false;
  cpu_t* c = mycpu();
  tcb_t* t = c->current_running;
  // since timer_needs_reset will only be set in do_scheduler
  // at that time the thread is already designated to run on this cpu
  spin_lock_acquire(&t->lock);
  if (t->status == TASK_EXITED) {
    // TODO: what should we do?
    spin_lock_release(&t->lock); 
  }
  else if (c->timer_needs_reset) {
    spin_lock_release(&t->lock);
    needs_reset = true;
  }
  else if (get_ticks() >= c->next_time) { // if it is just a timer interrupt
    t->status = TASK_READY;
    sched(t);
    c = mycpu();
    // after returning from do_yield this might be another CPU!
    needs_reset = true;
  } else {
    spin_lock_release(&t->lock);
  }
  assert(!spin_lock_holding(&t->lock));
  set_user_mode();
  w_stvec((uint64_t)exception_handler_entry);
  if (needs_reset) {
    c->next_time = get_ticks() + TIMER_INTERVAL;
    set_timer(c->next_time);
  }
  assert(holding_no_spin_lock());
  ret_from_exception(mythread()->trapframe);
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause) 
{
  do_yield();
}

void first_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause) 
{
  // barrier here
  static int cpu_cnt = 0;
  static spin_lock_t lock = { 
    .cpuid = -1,
    .status = UNLOCKED, 
  };
  bool has_entered_timer_irq = false;
  while(1) {
    spin_lock_acquire(&lock);
    if (cpu_cnt == NR_CPUS) {
      spin_lock_release(&lock);
      break;
    }
    if (!has_entered_timer_irq) {
      has_entered_timer_irq = true;
      cpu_cnt++;
    }
    spin_lock_release(&lock);
  }
  // after all the cpu leaves the barrier
  // we assume that in a short time they won't fall into timer interrupt again
  // so that when cpu 0 modifies the table, no one is reading it
  if (mycpuid() == 0)
    irq_table[IRQC_S_TIMER] = handle_irq_timer;
  do_scheduler();
}

void init_exception() 
{
  irq_table[IRQC_S_TIMER] = first_irq_timer;
  irq_table[IRQC_U_TIMER] = handle_irq_timer;
  exc_table[EXCC_SYSCALL] = handle_syscall;
  exc_table[EXCC_BREAKPOINT] = handle_other;
  exc_table[EXCC_INST_MISALIGNED] = handle_other;
  exc_table[EXCC_INST_ACCESS] = handle_other;
  exc_table[EXCC_INST_PAGE_FAULT] = handle_other;
  exc_table[EXCC_LOAD_ACCESS] = handle_other;
  exc_table[EXCC_LOAD_PAGE_FAULT] = handle_other;
  exc_table[EXCC_STORE_PAGE_FAULT] = handle_other;
  setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause) 
{
  char *reg_name[] = {
      "zero ", " ra  ", " sp  ", " gp  ", " tp  ", " t0  ", " t1  ", " t2  ",
      "s0/fp", " s1  ", " a0  ", " a1  ", " a2  ", " a3  ", " a4  ", " a5  ",
      " a6  ", " a7  ", " s2  ", " s3  ", " s4  ", " s5  ", " s6  ", " s7  ",
      " s8  ", " s9  ", " s10 ", " s11 ", " t3  ", " t4  ", " t5  ", " t6  "};
  for (int i = 0; i < 32; i += 3) {
    for (int j = 0; j < 3 && i + j < 32; ++j) {
      printk("%s : %016lx ", reg_name[i + j], regs->regs[i + j]);
    }
    printk("\n\r");
  }
  printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r", regs->sstatus,
         regs->sbadaddr, regs->scause);
  printk("sepc: 0x%lx\n\r", regs->sepc);
  printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
  while (1);
}
