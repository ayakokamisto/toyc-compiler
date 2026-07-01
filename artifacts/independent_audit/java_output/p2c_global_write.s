.data
.globl g
g:
    .word 1
.text
.globl main
main:
    addi sp, sp, -16
    sw ra, 12(sp)
    sw s0, 8(sp)
    sw s1, 4(sp)
    mv s0, sp
main_entry_0:
    la t1, g
    lw s1, 0(t1)
    mv t0, s1
    addi s1, t0, 41
    mv t0, s1
    la t1, g
    sw t0, 0(t1)
    mv a0, s1
    mv sp, s0
    lw ra, 12(sp)
    lw s0, 8(sp)
    lw s1, 4(sp)
    addi sp, sp, 16
    ret
