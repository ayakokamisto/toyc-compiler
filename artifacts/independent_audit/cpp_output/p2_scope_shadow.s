.text
.global main
main:
entry.0:
addi sp, sp, -48
  sw ra, 44(sp)
  sw s0, 40(sp)
  mv s0, sp
  
  
  li t0, 2
  sw t0, 0(s0)
  lw t0, 0(s0)
  sw t0, 24(s0)
  li t0, 7
  sw t0, 4(s0)
  lw t0, 4(s0)
  sw t0, 28(s0)
  lw t0, 28(s0)
  sw t0, 8(s0)
  li t0, 1
  sw t0, 12(s0)
  lw t0, 8(s0)
  lw t1, 12(s0)
  add t2, t0, t1
  sw t2, 16(s0)
  lw t0, 16(s0)
  sw t0, 28(s0)
  lw t0, 24(s0)
  sw t0, 20(s0)
  lw a0, 20(s0)
  mv sp, s0
  lw ra, 44(sp)
  lw s0, 40(sp)
  addi sp, sp, 48
  ret
