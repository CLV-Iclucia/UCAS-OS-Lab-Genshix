#include <assert.h>
#include <common.h>
#include <debugs.h>
#include <os/csr.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/time.h>
#include <printk.h>
#include <screen.h>
#include <stdbool.h>
extern bool timer_needs_reset;
handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void dump_kernel() 
{

}

static inline bool is_interrupt(uint64_t scause) { return (scause >> 63) != 0; }

static inline uint64_t exception_code(uint64_t scause) { return scause & 0xfff; }

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause) 
{
  timer_needs_reset = false;
  if (is_supervisor_mode()) {
    log(INTR, "Interrupt: scause: %lx, stval: %lx in supervisor mode", scause, stval);
    if (is_interrupt(scause)) {
      irq_table[exception_code(scause)](regs, stval, scause);
    } else {
      dump_kernel();
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
}

void user_trap_ret() 
{
  bool needs_reset = false;
  if (timer_needs_reset)
    needs_reset = true;
  else if (get_ticks() >= next_time) {
    do_yield();
    needs_reset = true;
  }
  if (needs_reset) {
    next_time = get_ticks() + TIMER_INTERVAL;
    set_timer(next_time);
  }
  set_user_mode();
  w_stvec((uint64_t)exception_handler_entry);
  update_interrupt();
  ret_from_exception(current_running->trapframe);
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause) 
{
  do_yield();
}

void first_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause) 
{
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
  exc_table[EXCC_UNKNOWN_EXCEPTION]= handle_other;
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
