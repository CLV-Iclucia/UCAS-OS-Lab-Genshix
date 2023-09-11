.global main
main:
    li  a0, 0     
    li  t0, 1   
    li  t1, 100
loop:
    add a0, a0, t0
    addi t0, t0, 1
    bge t0, t1, end
    j loop
end:
    j end
