.text
.globl fact
fact:
    addi sp, sp, -32
    sw ra, 28(sp)
    sw s0, 24(sp)
    mv s0, sp
    sw a0, 0(s0)
fact_entry_0:
    lw t0, 0(s0)
    li t1, 1
    bge t1, t0, if_then_1
    j if_end_2
if_then_1:
    li a0, 1
    mv sp, s0
    lw ra, 28(sp)
    lw s0, 24(sp)
    addi sp, sp, 32
    ret
if_end_2:
    lw t0, 0(s0)
    addi t2, t0, -1
    sw t2, 8(s0)
    lw a0, 8(s0)
    call fact
    sw a0, 12(s0)
    lw t0, 0(s0)
    lw t1, 12(s0)
    mul t2, t0, t1
    sw t2, 16(s0)
    lw a0, 16(s0)
    mv sp, s0
    lw ra, 28(sp)
    lw s0, 24(sp)
    addi sp, sp, 32
    ret
.globl main
main:
    addi sp, sp, -16
    sw ra, 12(sp)
    sw s0, 8(sp)
    mv s0, sp
main_entry_3:
    li a0, 5
    call fact
    sw a0, 0(s0)
    lw a0, 0(s0)
    mv sp, s0
    lw ra, 12(sp)
    lw s0, 8(sp)
    addi sp, sp, 16
    ret
