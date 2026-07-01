.text
.global main
main:
entry.0:
addi sp, sp, -96
  sw ra, 92(sp)
  sw s0, 88(sp)
  mv s0, sp
  
  
  li t0, 0
  sw t0, 0(s0)
  lw t0, 0(s0)
  sw t0, 76(s0)
  li t0, 0
  sw t0, 4(s0)
  lw t0, 4(s0)
  sw t0, 80(s0)
  j while.cond.1
while.cond.1:
  lw t0, 76(s0)
  sw t0, 8(s0)
  li t0, 10
  sw t0, 12(s0)
  lw t0, 8(s0)
  lw t1, 12(s0)
  slt t2, t0, t1
  sw t2, 16(s0)
  lw t0, 16(s0)
  li t1, 0
  xor t2, t0, t1
  snez t2, t2
  sw t2, 20(s0)
  lw t0, 20(s0)
  bnez t0, while.body.2
  j while.end.3
while.body.2:
  lw t0, 76(s0)
  sw t0, 24(s0)
  li t0, 1
  sw t0, 28(s0)
  lw t0, 24(s0)
  lw t1, 28(s0)
  add t2, t0, t1
  sw t2, 32(s0)
  lw t0, 32(s0)
  sw t0, 76(s0)
  lw t0, 76(s0)
  sw t0, 36(s0)
  li t0, 2
  sw t0, 40(s0)
  lw t0, 36(s0)
  lw t1, 40(s0)
  rem t2, t0, t1
  sw t2, 44(s0)
  li t0, 0
  sw t0, 48(s0)
  lw t0, 44(s0)
  lw t1, 48(s0)
  xor t2, t0, t1
  seqz t2, t2
  sw t2, 52(s0)
  lw t0, 52(s0)
  li t1, 0
  xor t2, t0, t1
  snez t2, t2
  sw t2, 56(s0)
  lw t0, 56(s0)
  bnez t0, if.then.4
  j if.end.5
while.end.3:
  lw t0, 80(s0)
  sw t0, 60(s0)
  lw a0, 60(s0)
  mv sp, s0
  lw ra, 92(sp)
  lw s0, 88(sp)
  addi sp, sp, 96
  ret
if.then.4:
  j while.cond.1
if.end.5:
  lw t0, 80(s0)
  sw t0, 64(s0)
  lw t0, 76(s0)
  sw t0, 68(s0)
  lw t0, 64(s0)
  lw t1, 68(s0)
  add t2, t0, t1
  sw t2, 72(s0)
  lw t0, 72(s0)
  sw t0, 80(s0)
  j while.cond.1
