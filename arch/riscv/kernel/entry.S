#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */
  // make sure that sscratch points to the pcb of current_running
  csrrw a0, sscratch, a0
  sd ra, OFFSET_REG_RA(a0)
  sd sp, OFFSET_REG_SP(a0)
  sd gp, OFFSET_REG_GP(a0)
  sd tp, OFFSET_REG_TP(a0)
  sd s0, OFFSET_REG_S0(a0)
  sd s1, OFFSET_REG_S1(a0)
  sd s2, OFFSET_REG_S2(a0)
  sd s3, OFFSET_REG_S3(a0)
  sd s4, OFFSET_REG_S4(a0)
  sd s5, OFFSET_REG_S5(a0)
  sd s6, OFFSET_REG_S6(a0)
  sd s7, OFFSET_REG_S7(a0)
  sd s8, OFFSET_REG_S8(a0)
  sd s9, OFFSET_REG_S9(a0)
  sd s10, OFFSET_REG_S10(a0)
  sd s11, OFFSET_REG_S11(a0)
  sd a1, OFFSET_REG_A1(a0)
  sd a2, OFFSET_REG_A2(a0)
  sd a3, OFFSET_REG_A3(a0)
  sd a4, OFFSET_REG_A4(a0)
  sd a5, OFFSET_REG_A5(a0)
  sd a6, OFFSET_REG_A6(a0)
  sd a7, OFFSET_REG_A7(a0)
  sd t0, OFFSET_REG_T0(a0)
  sd t1, OFFSET_REG_T1(a0)
  sd t2, OFFSET_REG_T2(a0)
  sd t3, OFFSET_REG_T3(a0)
  sd t4, OFFSET_REG_T4(a0)
  sd t5, OFFSET_REG_T5(a0)
  sd t6, OFFSET_REG_T6(a0)
  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */
  csrr t0, sstatus
  sd t0, OFFSET_REG_SSTATUS(a0)
  csrr t0, sepc
  sd t0, OFFSET_REG_SEPC(a0)
  csrr t0, stval
  sd t0, OFFSET_REG_SBADADDR(a0)
  csrr t0, scause
  sd t0, OFFSET_REG_SCAUSE(a0)
  csrr t0, sscratch
  sd t0, OFFSET_REG_A0(a0)
.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  ld ra, OFFSET_REG_RA(a0)
  ld sp, OFFSET_REG_SP(a0)
  ld gp, OFFSET_REG_GP(a0)
  ld tp, OFFSET_REG_TP(a0)
  ld s0, OFFSET_REG_S0(a0)
  ld s1, OFFSET_REG_S1(a0)
  ld s2, OFFSET_REG_S2(a0)
  ld s3, OFFSET_REG_S3(a0)
  ld s4, OFFSET_REG_S4(a0)
  ld s5, OFFSET_REG_S5(a0)
  ld s6, OFFSET_REG_S6(a0)
  ld s7, OFFSET_REG_S7(a0)
  ld s8, OFFSET_REG_S8(a0)
  ld s9, OFFSET_REG_S9(a0)
  ld s10, OFFSET_REG_S10(a0)
  ld s11, OFFSET_REG_S11(a0)
  ld a1, OFFSET_REG_A1(a0)
  ld a2, OFFSET_REG_A2(a0)
  ld a3, OFFSET_REG_A3(a0)
  ld a4, OFFSET_REG_A4(a0)
  ld a5, OFFSET_REG_A5(a0)
  ld a6, OFFSET_REG_A6(a0)
  ld a7, OFFSET_REG_A7(a0)
  ld t1, OFFSET_REG_T1(a0)
  ld t2, OFFSET_REG_T2(a0)
  ld t3, OFFSET_REG_T3(a0)
  ld t4, OFFSET_REG_T4(a0)
  ld t5, OFFSET_REG_T5(a0)
  ld t6, OFFSET_REG_T6(a0)
  ld t0, OFFSET_REG_SSTATUS(a0)
  csrw sstatus, t0
  ld t0, OFFSET_REG_SEPC(a0)
  csrw sepc, t0
  ld t0, OFFSET_REG_A0(a0)
  csrw sscratch, t0
  ld t0, OFFSET_REG_T0(a0)
  csrrw a0, sscratch, a0
.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
//  addi sp, sp, -(SWITCH_TO_SIZE)

  /* TODO: [p2-task1] save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/
  // SAVE_CONTEXT
  // tp is the current_running
  // a0 is the previous pcb context
  // a1 is the next pcb context, it is the current_running
  // there should be 14 registers to be saved
  sd ra, SWITCH_TO_RA(a0)
  sd sp, SWITCH_TO_SP(a0)
  sd s0, SWITCH_TO_S0(a0)
  sd s1, SWITCH_TO_S1(a0)
  sd s2, SWITCH_TO_S2(a0)
  sd s3, SWITCH_TO_S3(a0)
  sd s4, SWITCH_TO_S4(a0)
  sd s5, SWITCH_TO_S5(a0)
  sd s6, SWITCH_TO_S6(a0)
  sd s7, SWITCH_TO_S7(a0)
  sd s8, SWITCH_TO_S8(a0)
  sd s9, SWITCH_TO_S9(a0)
  sd s10, SWITCH_TO_S10(a0)
  sd s11, SWITCH_TO_S11(a0)
  
  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/
  // RESTORE_CONTEXT
  ld ra, SWITCH_TO_RA(a1)
  ld sp, SWITCH_TO_SP(a1)
  ld s0, SWITCH_TO_S0(a1)
  ld s1, SWITCH_TO_S1(a1)
  ld s2, SWITCH_TO_S2(a1)
  ld s3, SWITCH_TO_S3(a1)
  ld s4, SWITCH_TO_S4(a1)
  ld s5, SWITCH_TO_S5(a1)
  ld s6, SWITCH_TO_S6(a1)
  ld s7, SWITCH_TO_S7(a1)
  ld s8, SWITCH_TO_S8(a1)
  ld s9, SWITCH_TO_S9(a1)
  ld s10, SWITCH_TO_S10(a1)
  ld s11, SWITCH_TO_S11(a1)

//  addi sp, sp, SWITCH_TO_SIZE
  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
  RESTORE_CONTEXT
  sret
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)
  /* TODO: [p2-task3] save context via the provided macro */
  SAVE_CONTEXT
  la ra, user_trap_ret
  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */
  csrr a1, stval
  csrr a2, scause
  // set sp to kernel stack: PCB_KERNEL_SP(a0)
  addi t0, a0, OFFSET_SIZE
  ld sp, PCB_KERNEL_SP(t0)
  j interrupt_helper
ENDPROC(exception_handler_entry)

ENTRY(kernel_exception_handler)
  // save the context of kernel on current kernel stack
  addi sp, sp, -(OFFSET_SIZE)
  sd ra, OFFSET_REG_RA(sp)
  sd sp, OFFSET_REG_SP(sp)
  sd gp, OFFSET_REG_GP(sp)
  sd tp, OFFSET_REG_TP(sp)
  sd s0, OFFSET_REG_S0(sp)
  sd s1, OFFSET_REG_S1(sp)
  sd s2, OFFSET_REG_S2(sp)
  sd s3, OFFSET_REG_S3(sp)
  sd s4, OFFSET_REG_S4(sp)
  sd s5, OFFSET_REG_S5(sp)
  sd s6, OFFSET_REG_S6(sp)
  sd s7, OFFSET_REG_S7(sp)
  sd s8, OFFSET_REG_S8(sp)  
  sd s9, OFFSET_REG_S9(sp)
  sd s10, OFFSET_REG_S10(sp)
  sd s11, OFFSET_REG_S11(sp)
  sd a1, OFFSET_REG_A1(sp)
  sd a2, OFFSET_REG_A2(sp)
  sd a3, OFFSET_REG_A3(sp)
  sd a4, OFFSET_REG_A4(sp)
  sd a5, OFFSET_REG_A5(sp)
  sd a6, OFFSET_REG_A6(sp)
  sd a7, OFFSET_REG_A7(sp)
  sd t0, OFFSET_REG_T0(sp)
  sd t1, OFFSET_REG_T1(sp)
  sd t2, OFFSET_REG_T2(sp)
  sd t3, OFFSET_REG_T3(sp)
  sd t4, OFFSET_REG_T4(sp)
  sd t5, OFFSET_REG_T5(sp)
  sd t6, OFFSET_REG_T6(sp)
  
  la ra, kernel_trap_ret
  csrr a1, stval
  csrr a2, scause
  addi t0, a0, OFFSET_SIZE
  ld sp, PCB_KERNEL_SP(t0)
  j interrupt_helper
ENDPROC(kernel_exception_handler)

ENTRY(kernel_trap_ret)
  
ENDPROC(kernel_trap_ret)
