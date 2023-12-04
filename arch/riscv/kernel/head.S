/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

// at this time, the virtual address is already enabled
// so we can must use virtual address
#define KERNEL_STACK_0_KVA		0xffffffc050500000
#define KERNEL_STACK_1_KVA    0xffffffc050502000

.section ".entry_function","ax"
ENTRY(_start)
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero
  csrr t0, CSR_MHARTID
  li t1, 1
  beq t0, t1, hart_1
  la t0, __bss_start
  la t1, __BSS_END__
clear_loop:
  beq t0, t1, end_loop
  sb x0, 0(t0)
  addi t0, t0, 1
  j clear_loop
end_loop:
  li sp, KERNEL_STACK_0_KVA
  j main

hart_1:
  li sp, KERNEL_STACK_1_KVA
  j main
loop:
  wfi
  j loop

END(_start)
