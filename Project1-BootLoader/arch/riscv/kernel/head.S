/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

_bss_start_: 
  .word __bss_start

_bss_end_: 
  .word __BSS_END__

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  /* now I can only hardcode this */
  lui t0, %hi(_bss_start_)
  add t0, t0, %lo(_bss_start_)
  lw t0, 0(t0)
  lui t1, %hi(_bss_end_)
  add t1, t1, %lo(_bss_end_)
  lw t1, 0(t1)
clear_loop:
  beq t0, t1, end_loop
  sw x0, 0(t0)
  addi t0, t0, 1
  j clear_loop
end_loop:
  /* TODO: [p1-task2] setup C environment */
  li sp, KERNEL_STACK
  la t0, main
  jr t0
loop:
  wfi
  j loop

END(_start)
