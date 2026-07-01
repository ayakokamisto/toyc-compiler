.data
.globl g
g:
    .word 3
.text
.globl main
main:
    addi sp, sp, -32
    sw ra, 28(sp)
    sw s0, 24(sp)
    mv s0, sp
main_entry_1:
    la t0, g
    sw t0, 0(s0)
    lw t1, 0(s0)
    lw t0, 0(t1)
    sw t0, 4(s0)
    lw t0, 4(s0)
    addi t2, t0, 39
    sw t2, 8(s0)
    lw a0, 8(s0)
    mv sp, s0
    lw ra, 28(sp)
    lw s0, 24(sp)
    addi sp, sp, 32
    ret
