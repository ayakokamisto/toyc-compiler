.text
.global main
main:
entry.0:
addi sp, sp, -48
  sw ra, 44(sp)
  sw s0, 40(sp)
  mv s0, sp
  li t0, 17
  sw t0, 0(s0)
  li t0, 5
  sw t0, 4(s0)
  lw t0, 0(s0)
  lw t1, 4(s0)
  div t2, t0, t1
  sw t2, 8(s0)
  li t0, 17
  sw t0, 12(s0)
  li t0, 5
  sw t0, 16(s0)
  lw t0, 12(s0)
  lw t1, 16(s0)
  rem t2, t0, t1
  sw t2, 20(s0)
  lw t0, 8(s0)
  lw t1, 20(s0)
  add t2, t0, t1
  sw t2, 24(s0)
  lw a0, 24(s0)
  mv sp, s0
  lw ra, 44(sp)
  lw s0, 40(sp)
  addi sp, sp, 48
  ret
