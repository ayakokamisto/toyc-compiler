.text
.globl main
main:
    addi sp, sp, -48
    sw ra, 44(sp)
    sw s0, 40(sp)
    sw s1, 36(sp)
    sw s2, 32(sp)
    sw s3, 28(sp)
    sw s4, 24(sp)
    sw s5, 20(sp)
    sw s6, 16(sp)
    sw s7, 12(sp)
    mv s0, sp
main_entry_0:
    li s3, 0
    li s1, 0
    j while_cond_1
while_cond_1:
    mv t0, s3
    li t1, 10
    blt t0, t1, while_body_2
    j while_end_3
while_body_2:
    mv t0, s3
    addi s2, t0, 1
    mv t0, s2
    andi s4, t0, 1
    mv t0, s4
    addi s5, t0, 0
    seqz s5, s5
    mv t0, s2
    mv s3, t0
    mv t0, s5
    bnez t0, while_cond_1
    j if_end_5
while_end_3:
    mv a0, s1
    mv sp, s0
    lw ra, 44(sp)
    lw s0, 40(sp)
    lw s1, 36(sp)
    lw s2, 32(sp)
    lw s3, 28(sp)
    lw s4, 24(sp)
    lw s5, 20(sp)
    lw s6, 16(sp)
    lw s7, 12(sp)
    addi sp, sp, 48
    ret
if_end_5:
    mv t0, s1
    mv t1, s2
    add s6, t0, t1
    mv t0, s2
    mv s3, t0
    mv t0, s6
    mv s1, t0
    j while_cond_1
