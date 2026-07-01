.text
.globl main
main:
    addi sp, sp, -16
    sw ra, 12(sp)
    sw s0, 8(sp)
    mv s0, sp
main_entry_2:
    li a0, 42
    mv sp, s0
    lw ra, 12(sp)
    lw s0, 8(sp)
    addi sp, sp, 16
    ret
