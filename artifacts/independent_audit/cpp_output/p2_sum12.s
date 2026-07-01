.text
.global sum12
sum12:
entry.0:
addi sp, sp, -160
  sw ra, 156(sp)
  sw s0, 152(sp)
  mv s0, sp
  sw a0, 92(s0)
  sw a1, 96(s0)
  sw a2, 100(s0)
  sw a3, 104(s0)
  sw a4, 108(s0)
  sw a5, 112(s0)
  sw a6, 116(s0)
  sw a7, 120(s0)
lw t0, 160(s0)
  sw t0, 124(s0)
lw t0, 164(s0)
  sw t0, 128(s0)
lw t0, 168(s0)
  sw t0, 132(s0)
lw t0, 172(s0)
  sw t0, 136(s0)
  
  
  
  
  
  
  
  
  
  
  
  
  lw t0, 92(s0)
  sw t0, 0(s0)
  lw t0, 96(s0)
  sw t0, 4(s0)
  lw t0, 0(s0)
  lw t1, 4(s0)
  add t2, t0, t1
  sw t2, 8(s0)
  lw t0, 100(s0)
  sw t0, 12(s0)
  lw t0, 8(s0)
  lw t1, 12(s0)
  add t2, t0, t1
  sw t2, 16(s0)
  lw t0, 104(s0)
  sw t0, 20(s0)
  lw t0, 16(s0)
  lw t1, 20(s0)
  add t2, t0, t1
  sw t2, 24(s0)
  lw t0, 108(s0)
  sw t0, 28(s0)
  lw t0, 24(s0)
  lw t1, 28(s0)
  add t2, t0, t1
  sw t2, 32(s0)
  lw t0, 112(s0)
  sw t0, 36(s0)
  lw t0, 32(s0)
  lw t1, 36(s0)
  add t2, t0, t1
  sw t2, 40(s0)
  lw t0, 116(s0)
  sw t0, 44(s0)
  lw t0, 40(s0)
  lw t1, 44(s0)
  add t2, t0, t1
  sw t2, 48(s0)
  lw t0, 120(s0)
  sw t0, 52(s0)
  lw t0, 48(s0)
  lw t1, 52(s0)
  add t2, t0, t1
  sw t2, 56(s0)
  lw t0, 124(s0)
  sw t0, 60(s0)
  lw t0, 56(s0)
  lw t1, 60(s0)
  add t2, t0, t1
  sw t2, 64(s0)
  lw t0, 128(s0)
  sw t0, 68(s0)
  lw t0, 64(s0)
  lw t1, 68(s0)
  add t2, t0, t1
  sw t2, 72(s0)
  lw t0, 132(s0)
  sw t0, 76(s0)
  lw t0, 72(s0)
  lw t1, 76(s0)
  add t2, t0, t1
  sw t2, 80(s0)
  lw t0, 136(s0)
  sw t0, 84(s0)
  lw t0, 80(s0)
  lw t1, 84(s0)
  add t2, t0, t1
  sw t2, 88(s0)
  lw a0, 88(s0)
  mv sp, s0
  lw ra, 156(sp)
  lw s0, 152(sp)
  addi sp, sp, 160
  ret
.global main
main:
entry.0:
addi sp, sp, -80
  sw ra, 76(sp)
  sw s0, 72(sp)
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
  li t0, 10
  sw t0, 36(s0)
  li t0, 11
  sw t0, 40(s0)
  li t0, 12
  sw t0, 44(s0)
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
  lw t0, 36(s0)
  sw t0, 4(s0)
  lw t0, 40(s0)
  sw t0, 8(s0)
  lw t0, 44(s0)
  sw t0, 12(s0)
  call sum12
  sw a0, 48(s0)
  lw a0, 48(s0)
  mv sp, s0
  lw ra, 76(sp)
  lw s0, 72(sp)
  addi sp, sp, 80
  ret
