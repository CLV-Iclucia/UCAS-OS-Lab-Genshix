#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001f8
.equ meta_offset_loc, 0x502001f4
.equ user_start_offset_loc, 0x502001f0

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000
.equ decompressor_offset, 0x1000
.equ decompressor_loc, 0x52000000
.equ kernel, 0x50201000
.equ decompressor_stack, 0x50500000
.text
.global main

main:
	// fence on all memory and I/O
	fence
	// [p1-task1] call BIOS to print string "It's bootblock!"
    li a7, 9
    la a0, msg
    jal bios_func_entry
	// load decompressor
  // decompressor_size = user_start_offset - os_size - 512
    la t0, user_start_offset_loc
    lw s2, 0(t0)
    la t0, os_size_loc
    lw t1, 0(t0)
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
    lw a1, 0(t0)
    la t0, decompressor_loc
    jalr ra, 0(t0)
    bnez a0, done
    li a7, 9
    la a0, err_msg
    jal bios_func_entry
    j kernel
done:
  // before jumping to the kernel, remove the first instruction of decompressor
  // so that if we execute the decompressor in the future by mistake it will immmediately stop
    li a7, 9
    la a0, done_msg
    jal bios_func_entry
    la t0, decompressor_loc
    sw zero, 0(t0)
  // now we can jump to the kernel
    j kernel
	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"
err_msg: .string "Decompression failed...\n\r"
done_msg: .string "Decompression done.\n\r"
