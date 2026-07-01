.text
.global main
main:
entry.0:
addi sp, sp, -32
  sw ra, 28(sp)
  sw s0, 24(sp)
  mv s0, sp
  li t0, 1
  sw t0, 0(s0)
  li t0, 2
  sw t0, 4(s0)
  li t0, 3
  sw t0, 8(s0)
  lw t0, 4(s0)
  lw t1, 8(s0)
  mul t2, t0, t1
  sw t2, 12(s0)
  lw t0, 0(s0)
  lw t1, 12(s0)
  add t2, t0, t1
  sw t2, 16(s0)
  lw a0, 16(s0)
  mv sp, s0
  lw ra, 28(sp)
  lw s0, 24(sp)
  addi sp, sp, 32
  ret
