/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK_0		0x50500000
#define KERNEL_STACK_1    0x50502000

.section ".entry_function","ax"
ENTRY(_start)
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero
  csrr t0, CSR_MHARTID
  li t1, 1
  beq t0, t1, hart_1
  lui t0, %hi(__bss_start)
  add t0, t0, %lo(__bss_start)
  lui t1, %hi(__BSS_END__)
  add t1, t1, %lo(__BSS_END__)
clear_loop:
  beq t0, t1, end_loop
  sb x0, 0(t0)
  addi t0, t0, 1
  j clear_loop
end_loop:
  li sp, KERNEL_STACK_0
  j main

hart_1:
  li sp, KERNEL_STACK_1
  j main
loop:
  wfi
  j loop

END(_start)
