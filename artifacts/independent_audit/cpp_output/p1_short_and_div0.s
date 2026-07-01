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
  sw t0, 24(s0)
  j land.end.2
land.rhs.1:
  li t0, 1
  sw t0, 4(s0)
  li t0, 0
  sw t0, 8(s0)
  lw t0, 4(s0)
  lw t1, 8(s0)
  div t2, t0, t1
  sw t2, 12(s0)
  lw t0, 12(s0)
  li t1, 0
  xor t2, t0, t1
  snez t2, t2
  sw t2, 16(s0)
  lw t0, 16(s0)
  sw t0, 24(s0)
  j land.end.2
land.end.2:
  lw t0, 24(s0)
  sw t0, 20(s0)
  lw a0, 20(s0)
  mv sp, s0
  lw ra, 44(sp)
  lw s0, 40(sp)
  addi sp, sp, 48
  ret
