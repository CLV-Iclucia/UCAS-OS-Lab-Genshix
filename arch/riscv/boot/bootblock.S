#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001f8
.equ meta_offset_loc, 0x502001f4
.equ user_start_offset_loc, 0x502001f0

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000
# .equ decompressor_offset, 0x1000
# .equ decompressor_loc, 0x54000000
.equ kernel, 0x50202000
# .equ decompressor_stack, 0x50500000

.text
.global main

main:
	// fence on all memory and I/O
	fence
  // if a0 = 1, branch to booting_cpu1
  // else, branch to booting_cpu0
  li t0, 1
  beq a0, t0, booting_cpu1
  j booting_cpu0
booting_cpu0: 
    la      a5, os_size_loc
    lw      s1,0(a5)
    ld      a5,0(a5)
    srliw   a5,s1,9
    sext.w  s4,a5
    li      s1,0
    li      s3,1
    j       .L2
.L4:
    subw    a5,s4,s1
    sext.w  a5,a5
    sext.w  a3,a5
    li      a4,64
    bleu    a3,a4,.L3
    li      a5,64
.L3:
    sext.w  s2,a5
    la      a4,kernel
    slliw   a5,s1,9
    sext.w  a5,a5
    slli    a5,a5,32
    srli    a5,a5,32
    add     a5,a4,a5
    mv      a2,s3
    mv      a1,s2
    mv      a0,a5
    li      a7, BIOS_SDREAD
    jal     bios_func_entry
    addw    a5,s1,s2
    sext.w  s1,a5
    addw    a5,s3,s2
    sext.w  s3,a5
.L2:
    mv      a4,s1
    mv      a5,s4
    bltu    a4,a5,.L4
    li      a5,0
    mv      a0,a5
    j       kernel
	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

booting_cpu1:
  la t0, kernel
  csrw stvec, t0
  not t0, x0
  csrs 0x104, t0
  csrr t0, sstatus
  ori t0, t0, 2
  csrw sstatus, t0
loop:
  j loop