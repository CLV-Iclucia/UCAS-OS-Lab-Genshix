#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001f8
.equ meta_offset_loc, 0x502001f4
.equ user_start_offset_loc, 0x502001f0

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000
.equ decompressor_offset, 0x1000
.equ decompressor_loc, 0x54000000
.equ kernel, 0x50201000
.equ decompressor_stack, 0x50500000

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
    li a7, 9
    la a0, cpu0_msg
    jal bios_func_entry
  // decompressor_size = user_start_offset - os_size - 512
    la t0, user_start_offset_loc
    ld s2, 0(t0)
    la t0, os_size_loc
    ld t1, 0(t0)
    sub s2, s2, t1     // s2 is the kernel offset
    addi t0, s2, -512
    srai a1, t0, 9 // #sectors
    li a7, BIOS_SDREAD
    la a0, decompressor_loc
    li a2, 1        // sector number: decompressor will always be put after bootblock
    jal bios_func_entry
  // decompress(uint32_t start, uint 32_t size)
    la sp, decompressor_stack
    mv a0, s2
    la t0, os_size_loc
    ld a1, 0(t0)
    la t0, decompressor_loc
    jalr ra, 0(t0)
    bnez a0, done
    j kernel
done:
  // before jumping to the kernel, remove the first instruction of decompressor
  // so that if we execute the decompressor in the future by mistake it will immmediately stop
    la t0, decompressor_loc
    sd zero, 0(t0)
  // now we can jump to the kernel
    j kernel
	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

booting_cpu1:
  li a7, 9
  la a0, cpu1_msg
  jal bios_func_entry
  la t0, kernel
  csrw stvec, t0
  not t0, x0
  csrs 0x104, t0
  csrr t0, sstatus
  ori t0, t0, 2
  csrw sstatus, t0
loop:
  wfi
  j loop
.data

cpu0_msg: .string "CPU 0 booting\n\r"
cpu1_msg: .string "CPU 1 booting\n\r"