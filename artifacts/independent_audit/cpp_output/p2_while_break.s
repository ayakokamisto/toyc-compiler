.text
.global main
main:
entry.0:
addi sp, sp, -48
  sw ra, 44(sp)
  sw s0, 40(sp)
  mv s0, sp
  
  li t0, 0
  sw t0, 0(s0)
  lw t0, 0(s0)
  sw t0, 36(s0)
  j while.cond.1
while.cond.1:
  j while.body.2
while.body.2:
  lw t0, 36(s0)
  sw t0, 4(s0)
  li t0, 1
  sw t0, 8(s0)
  lw t0, 4(s0)
  lw t1, 8(s0)
  add t2, t0, t1
  sw t2, 12(s0)
  lw t0, 12(s0)
  sw t0, 36(s0)
  lw t0, 36(s0)
  sw t0, 16(s0)
  li t0, 3
  sw t0, 20(s0)
  lw t0, 16(s0)
  lw t1, 20(s0)
  xor t2, t0, t1
  seqz t2, t2
  sw t2, 24(s0)
  lw t0, 24(s0)
  li t1, 0
  xor t2, t0, t1
  snez t2, t2
  sw t2, 28(s0)
  lw t0, 28(s0)
  bnez t0, if.then.4
  j if.end.5
while.end.3:
  lw t0, 36(s0)
  sw t0, 32(s0)
  lw a0, 32(s0)
  mv sp, s0
  lw ra, 44(sp)
  lw s0, 40(sp)
  addi sp, sp, 48
  ret
if.then.4:
  j while.end.3
if.end.5:
  j while.cond.1
