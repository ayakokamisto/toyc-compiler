.text
.global main
main:
entry.0:
addi sp, sp, -16
  sw ra, 12(sp)
  sw s0, 8(sp)
  mv s0, sp
  li t0, 0
  sw t0, 0(s0)
  lw a0, 0(s0)
  mv sp, s0
  lw ra, 12(sp)
  lw s0, 8(sp)
  addi sp, sp, 16
  ret
