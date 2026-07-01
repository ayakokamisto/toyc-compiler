.text
.global sum9
sum9:
entry.0:
addi sp, sp, -112
  sw ra, 108(sp)
  sw s0, 104(sp)
  mv s0, sp
  sw a0, 68(s0)
  sw a1, 72(s0)
  sw a2, 76(s0)
  sw a3, 80(s0)
  sw a4, 84(s0)
  sw a5, 88(s0)
  sw a6, 92(s0)
  sw a7, 96(s0)
lw t0, 112(s0)
  sw t0, 100(s0)
  
  
  
  
  
  
  
  
  
  lw t0, 68(s0)
  sw t0, 0(s0)
  lw t0, 72(s0)
  sw t0, 4(s0)
  lw t0, 0(s0)
  lw t1, 4(s0)
  add t2, t0, t1
  sw t2, 8(s0)
  lw t0, 76(s0)
  sw t0, 12(s0)
  lw t0, 8(s0)
  lw t1, 12(s0)
  add t2, t0, t1
  sw t2, 16(s0)
  lw t0, 80(s0)
  sw t0, 20(s0)
  lw t0, 16(s0)
  lw t1, 20(s0)
  add t2, t0, t1
  sw t2, 24(s0)
  lw t0, 84(s0)
  sw t0, 28(s0)
  lw t0, 24(s0)
  lw t1, 28(s0)
  add t2, t0, t1
  sw t2, 32(s0)
  lw t0, 88(s0)
  sw t0, 36(s0)
  lw t0, 32(s0)
  lw t1, 36(s0)
  add t2, t0, t1
  sw t2, 40(s0)
  lw t0, 92(s0)
  sw t0, 44(s0)
  lw t0, 40(s0)
  lw t1, 44(s0)
  add t2, t0, t1
  sw t2, 48(s0)
  lw t0, 96(s0)
  sw t0, 52(s0)
  lw t0, 48(s0)
  lw t1, 52(s0)
  add t2, t0, t1
  sw t2, 56(s0)
  lw t0, 100(s0)
  sw t0, 60(s0)
  lw t0, 56(s0)
  lw t1, 60(s0)
  add t2, t0, t1
  sw t2, 64(s0)
  lw a0, 64(s0)
  mv sp, s0
  lw ra, 108(sp)
  lw s0, 104(sp)
  addi sp, sp, 112
  ret
.global main
main:
entry.0:
addi sp, sp, -64
  sw ra, 60(sp)
  sw s0, 56(sp)
  mv s0, sp
  li t0, 1
  sw t0, 0(s0)
  li t0, 2
  sw t0, 4(s0)
  li t0, 3
  sw t0, 8(s0)
  li t0, 4
  sw t0, 12(s0)
  li t0, 5
  sw t0, 16(s0)
  li t0, 6
  sw t0, 20(s0)
  li t0, 7
  sw t0, 24(s0)
  li t0, 8
  sw t0, 28(s0)
  li t0, 9
  sw t0, 32(s0)
  lw a0, 0(s0)
  lw a1, 4(s0)
  lw a2, 8(s0)
  lw a3, 12(s0)
  lw a4, 16(s0)
  lw a5, 20(s0)
  lw a6, 24(s0)
  lw a7, 28(s0)
  lw t0, 32(s0)
  sw t0, 0(s0)
  call sum9
  sw a0, 36(s0)
  lw a0, 36(s0)
  mv sp, s0
  lw ra, 60(sp)
  lw s0, 56(sp)
  addi sp, sp, 64
  ret
