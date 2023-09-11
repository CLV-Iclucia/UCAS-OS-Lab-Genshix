.global main

.text
li sp, 0x50000100 
main:
    addi sp, sp, -16
    sw  ra, 0(sp)   
    li  a0, 0 
    li  t0, 1   
    li  t1, 100
.loop:
    sw  a0, 4(sp)
    sw  t0, 8(sp)
    sw  t1, 12(sp)
    mv a0, t0
    call is_prime
    mv  t2, a0
    lw  a0, 4(sp)
    lw  t0, 8(sp)
    lw  t1, 12(sp)
    beqz t2, .inc
    add a0, a0, t0
.inc:
    addi t0, t0, 1
    bge t0, t1, .end
    j .loop
.end:
    addi sp, sp, 16
    j .end

is_prime:
.push:
    addi sp, sp, -8
    sw ra, 0(sp)
    sw a1, 4(sp)
.is_prime_impl:
    li t0, 1
    beq a0, t0, .return_false
#   t0 enum the factors
#   t1 = t0 * t0
    li t0, 2
    li t1, 4
.is_prime_loop:
    bgt t1, a0, .return_true
    remu a1, a0, t0
    beqz a1, .return_false
    addi t0, t0, 1
    mul t1, t0, t0
    j .is_prime_loop
.return_true:
    li a0, 1
    j .pop
.return_false:
    li a0, 0
    j .pop
.pop:
    lw ra, 0(sp)
    lw a1, 4(sp)
    addi sp, sp, 8
    ret
