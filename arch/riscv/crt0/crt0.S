#include <asm.h>

.section ".entry_function","ax"

ENTRY(_start)
    addi sp, sp, -8
    sd ra, 0(sp)
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
    call main
    // TODO[p3]: call exit
    
    ld ra, 0(sp)
    addi sp, sp, 8
    
    ret
    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
loop:
    wfi
    j loop

END(_start)
