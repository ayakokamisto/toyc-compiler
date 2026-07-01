.text
.globl sumDown9
sumDown9:
    addi sp, sp, -80
    sw ra, 76(sp)
    sw s0, 72(sp)
    sw s1, 68(sp)
    sw s2, 64(sp)
    sw s3, 60(sp)
    sw s4, 56(sp)
    sw s5, 52(sp)
    sw s6, 48(sp)
    sw s7, 44(sp)
    sw s8, 40(sp)
    sw s9, 36(sp)
    sw s10, 32(sp)
    sw s11, 28(sp)
    mv s0, sp
    mv s9, a0
    mv s1, a1
    mv s2, a2
    mv s3, a3
    mv s4, a4
    mv s5, a5
    mv s6, a6
    mv s7, a7
    lw t0, 80(s0)
    mv s8, t0
sumDown9_entry_0:
    mv t0, s9
    beqz t0, if_then_1
    j if_end_2
if_then_1:
    mv t0, s1
    mv t1, s2
    add t2, t0, t1
    sw t2, 0(s0)
    lw t0, 0(s0)
    mv t1, s3
    add t2, t0, t1
    sw t2, 4(s0)
    lw t0, 4(s0)
    mv t1, s4
    add t2, t0, t1
    sw t2, 8(s0)
    lw t0, 8(s0)
    mv t1, s5
    add s10, t0, t1
    mv t0, s10
    mv t1, s6
    add s11, t0, t1
    mv t0, s11
    mv t1, s7
    add t3, t0, t1
    mv t0, t3
    mv t1, s8
    add t4, t0, t1
    mv a0, t4
    mv sp, s0
    lw ra, 76(sp)
    lw s0, 72(sp)
    lw s1, 68(sp)
    lw s2, 64(sp)
    lw s3, 60(sp)
    lw s4, 56(sp)
    lw s5, 52(sp)
    lw s6, 48(sp)
    lw s7, 44(sp)
    lw s8, 40(sp)
    lw s9, 36(sp)
    lw s10, 32(sp)
    lw s11, 28(sp)
    addi sp, sp, 80
    ret
if_end_2:
    mv t0, s9
    addi a2, t0, -1
    mv t0, s1
    addi a3, t0, 1
    mv t0, s2
    addi a4, t0, 1
    mv t0, s3
    addi a5, t0, 1
    mv t0, s4
    addi a6, t0, 1
    mv t0, s5
    addi a7, t0, 1
    mv t0, s6
    addi t2, t0, 1
    sw t2, 12(s0)
    mv t0, s7
    addi t2, t0, 1
    sw t2, 16(s0)
    mv t0, s8
    addi t2, t0, 1
    sw t2, 20(s0)
    mv t0, a2
    mv s9, t0
    mv t0, a3
    mv s1, t0
    mv t0, a4
    mv s2, t0
    mv t0, a5
    mv s3, t0
    mv t0, a6
    mv s4, t0
    mv t0, a7
    mv s5, t0
    lw t0, 12(s0)
    mv s6, t0
    lw t0, 16(s0)
    mv s7, t0
    lw t0, 20(s0)
    mv s8, t0
    j sumDown9_entry_0
.globl main
main:
    addi sp, sp, -16
    sw ra, 12(sp)
    sw s0, 8(sp)
    mv s0, sp
main_entry_3:
    li a0, 2
    li a1, 1
    li a2, 1
    li a3, 1
    li a4, 1
    li a5, 1
    li a6, 1
    li a7, 1
    addi sp, sp, -16
    li t0, 1
    sw t0, 0(sp)
    call sumDown9
    addi sp, sp, 16
    sw a0, 0(s0)
    lw a0, 0(s0)
    mv sp, s0
    lw ra, 12(sp)
    lw s0, 8(sp)
    addi sp, sp, 16
    ret
