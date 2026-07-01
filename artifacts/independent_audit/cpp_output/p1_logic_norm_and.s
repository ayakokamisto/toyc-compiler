.text
.global main
main:
entry.0:
addi sp, sp, -32
  sw ra, 28(sp)
  sw s0, 24(sp)
  mv s0, sp
  
  li t0, 0
  sw t0, 0(s0)
  lw t0, 0(s0)
  sw t0, 16(s0)
  j land.rhs.1
land.rhs.1:
  li t0, 9
  sw t0, 4(s0)
  lw t0, 4(s0)
  li t1, 0
  xor t2, t0, t1
  snez t2, t2
  sw t2, 8(s0)
  lw t0, 8(s0)
  sw t0, 16(s0)
  j land.end.2
land.end.2:
  lw t0, 16(s0)
  sw t0, 12(s0)
  lw a0, 12(s0)
  mv sp, s0
  lw ra, 28(sp)
  lw s0, 24(sp)
  addi sp, sp, 32
  ret
