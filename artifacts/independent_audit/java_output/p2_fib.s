.text
.globl fib
fib:
    addi sp, sp, -48
    sw ra, 44(sp)
    sw s0, 40(sp)
    mv s0, sp
    sw a0, 0(s0)
fib_entry_0:
    lw t0, 0(s0)
    li t1, 1
    bge t1, t0, if_then_1
    j if_end_2
if_then_1:
    lw a0, 0(s0)
    mv sp, s0
    lw ra, 44(sp)
    lw s0, 40(sp)
    addi sp, sp, 48
    ret
if_end_2:
    lw t0, 0(s0)
    addi t2, t0, -1
    sw t2, 8(s0)
    lw a0, 8(s0)
    call fib
    sw a0, 12(s0)
    lw t0, 0(s0)
    addi t2, t0, -2
    sw t2, 16(s0)
    lw a0, 16(s0)
    call fib
    sw a0, 20(s0)
    lw t0, 12(s0)
    lw t1, 20(s0)
    add t2, t0, t1
    sw t2, 24(s0)
    lw a0, 24(s0)
    mv sp, s0
    lw ra, 44(sp)
    lw s0, 40(sp)
    addi sp, sp, 48
    ret
.globl main
main:
    addi sp, sp, -16
    sw ra, 12(sp)
    sw s0, 8(sp)
    mv s0, sp
main_entry_3:
    li a0, 8
    call fib
    sw a0, 0(s0)
    lw a0, 0(s0)
    mv sp, s0
    lw ra, 12(sp)
    lw s0, 8(sp)
    addi sp, sp, 16
    ret
