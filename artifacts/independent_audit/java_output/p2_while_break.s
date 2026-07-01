.text
.globl main
main:
    addi sp, sp, -32
    sw ra, 28(sp)
    sw s0, 24(sp)
    sw s1, 20(sp)
    sw s2, 16(sp)
    sw s3, 12(sp)
    mv s0, sp
main_entry_0:
    li s2, 0
    j while_body_2
while_body_2:
    mv t0, s2
    addi s1, t0, 1
    mv t0, s1
    addi s3, t0, -3
    seqz s3, s3
    mv t0, s1
    mv s2, t0
    mv t0, s3
    bnez t0, while_end_3
    j while_body_2
while_end_3:
    mv a0, s1
    mv sp, s0
    lw ra, 28(sp)
    lw s0, 24(sp)
    lw s1, 20(sp)
    lw s2, 16(sp)
    lw s3, 12(sp)
    addi sp, sp, 32
    ret
