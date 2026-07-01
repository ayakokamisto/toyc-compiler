.data
.global g
g:
  .word 7
.text
.global main
main:
entry.0:
addi sp, sp, -16
  sw ra, 12(sp)
  sw s0, 8(sp)
  mv s0, sp
  la t0, g
  sw t0, 0(s0)
  lw t0, 0(s0)
  lw t1, 0(t0)
  sw t1, 4(s0)
  lw a0, 4(s0)
  mv sp, s0
  lw ra, 12(sp)
  lw s0, 8(sp)
  addi sp, sp, 16
  ret
