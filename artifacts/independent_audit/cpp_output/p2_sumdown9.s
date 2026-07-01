.text
.global sumDown9
sumDown9:
entry.0:
addi sp, sp, -240
  sw ra, 236(sp)
  sw s0, 232(sp)
  mv s0, sp
  sw a0, 188(s0)
  sw a1, 192(s0)
  sw a2, 196(s0)
  sw a3, 200(s0)
  sw a4, 204(s0)
  sw a5, 208(s0)
  sw a6, 212(s0)
  sw a7, 216(s0)
lw t0, 240(s0)
  sw t0, 220(s0)
  
  
  
  
  
  
  
  
  
  lw t0, 188(s0)
  sw t0, 0(s0)
  li t0, 0
  sw t0, 4(s0)
  lw t0, 0(s0)
  lw t1, 4(s0)
  xor t2, t0, t1
  seqz t2, t2
  sw t2, 8(s0)
  lw t0, 8(s0)
  li t1, 0
  xor t2, t0, t1
  snez t2, t2
  sw t2, 12(s0)
  lw t0, 12(s0)
  bnez t0, if.then.1
  j if.end.2
if.then.1:
  lw t0, 192(s0)
  sw t0, 16(s0)
  lw t0, 196(s0)
  sw t0, 20(s0)
  lw t0, 16(s0)
  lw t1, 20(s0)
  add t2, t0, t1
  sw t2, 24(s0)
  lw t0, 200(s0)
  sw t0, 28(s0)
  lw t0, 24(s0)
  lw t1, 28(s0)
  add t2, t0, t1
  sw t2, 32(s0)
  lw t0, 204(s0)
  sw t0, 36(s0)
  lw t0, 32(s0)
  lw t1, 36(s0)
  add t2, t0, t1
  sw t2, 40(s0)
  lw t0, 208(s0)
  sw t0, 44(s0)
  lw t0, 40(s0)
  lw t1, 44(s0)
  add t2, t0, t1
  sw t2, 48(s0)
  lw t0, 212(s0)
  sw t0, 52(s0)
  lw t0, 48(s0)
  lw t1, 52(s0)
  add t2, t0, t1
  sw t2, 56(s0)
  lw t0, 216(s0)
  sw t0, 60(s0)
  lw t0, 56(s0)
  lw t1, 60(s0)
  add t2, t0, t1
  sw t2, 64(s0)
  lw t0, 220(s0)
  sw t0, 68(s0)
  lw t0, 64(s0)
  lw t1, 68(s0)
  add t2, t0, t1
  sw t2, 72(s0)
  lw a0, 72(s0)
  mv sp, s0
  lw ra, 236(sp)
  lw s0, 232(sp)
  addi sp, sp, 240
  ret
if.end.2:
  lw t0, 188(s0)
  sw t0, 76(s0)
  li t0, 1
  sw t0, 80(s0)
  lw t0, 76(s0)
  lw t1, 80(s0)
  sub t2, t0, t1
  sw t2, 84(s0)
  lw t0, 192(s0)
  sw t0, 88(s0)
  li t0, 1
  sw t0, 92(s0)
  lw t0, 88(s0)
  lw t1, 92(s0)
  add t2, t0, t1
  sw t2, 96(s0)
  lw t0, 196(s0)
  sw t0, 100(s0)
  li t0, 1
  sw t0, 104(s0)
  lw t0, 100(s0)
  lw t1, 104(s0)
  add t2, t0, t1
  sw t2, 108(s0)
  lw t0, 200(s0)
  sw t0, 112(s0)
  li t0, 1
  sw t0, 116(s0)
  lw t0, 112(s0)
  lw t1, 116(s0)
  add t2, t0, t1
  sw t2, 120(s0)
  lw t0, 204(s0)
  sw t0, 124(s0)
  li t0, 1
  sw t0, 128(s0)
  lw t0, 124(s0)
  lw t1, 128(s0)
  add t2, t0, t1
  sw t2, 132(s0)
  lw t0, 208(s0)
  sw t0, 136(s0)
  li t0, 1
  sw t0, 140(s0)
  lw t0, 136(s0)
  lw t1, 140(s0)
  add t2, t0, t1
  sw t2, 144(s0)
  lw t0, 212(s0)
  sw t0, 148(s0)
  li t0, 1
  sw t0, 152(s0)
  lw t0, 148(s0)
  lw t1, 152(s0)
  add t2, t0, t1
  sw t2, 156(s0)
  lw t0, 216(s0)
  sw t0, 160(s0)
  li t0, 1
  sw t0, 164(s0)
  lw t0, 160(s0)
  lw t1, 164(s0)
  add t2, t0, t1
  sw t2, 168(s0)
  lw t0, 220(s0)
  sw t0, 172(s0)
  li t0, 1
  sw t0, 176(s0)
  lw t0, 172(s0)
  lw t1, 176(s0)
  add t2, t0, t1
  sw t2, 180(s0)
  lw a0, 84(s0)
  lw a1, 96(s0)
  lw a2, 108(s0)
  lw a3, 120(s0)
  lw a4, 132(s0)
  lw a5, 144(s0)
  lw a6, 156(s0)
  lw a7, 168(s0)
  lw t0, 180(s0)
  sw t0, 0(s0)
  call sumDown9
  sw a0, 184(s0)
  lw a0, 184(s0)
  mv sp, s0
  lw ra, 236(sp)
  lw s0, 232(sp)
  addi sp, sp, 240
  ret
.global main
main:
entry.0:
addi sp, sp, -64
  sw ra, 60(sp)
  sw s0, 56(sp)
  mv s0, sp
  li t0, 2
  sw t0, 0(s0)
  li t0, 1
  sw t0, 4(s0)
  li t0, 1
  sw t0, 8(s0)
  li t0, 1
  sw t0, 12(s0)
  li t0, 1
  sw t0, 16(s0)
  li t0, 1
  sw t0, 20(s0)
  li t0, 1
  sw t0, 24(s0)
  li t0, 1
  sw t0, 28(s0)
  li t0, 1
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
  call sumDown9
  sw a0, 36(s0)
  lw a0, 36(s0)
  mv sp, s0
  lw ra, 60(sp)
  lw s0, 56(sp)
  addi sp, sp, 64
  ret
