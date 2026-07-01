.data
.global g
g:
  .word 1
.text
.global bump
bump:
entry.0:
addi sp, sp, -48
  sw ra, 44(sp)
  sw s0, 40(sp)
  mv s0, sp
  la t0, g
  sw t0, 0(s0)
  la t0, g
  sw t0, 4(s0)
  lw t0, 4(s0)
  lw t1, 0(t0)
  sw t1, 8(s0)
  li t0, 1
  sw t0, 12(s0)
  lw t0, 8(s0)
  lw t1, 12(s0)
  add t2, t0, t1
  sw t2, 16(s0)
  lw t0, 16(s0)
  lw t1, 0(s0)
  sw t0, 0(t1)
  la t0, g
  sw t0, 20(s0)
  lw t0, 20(s0)
  lw t1, 0(t0)
  sw t1, 24(s0)
  lw a0, 24(s0)
  mv sp, s0
  lw ra, 44(sp)
  lw s0, 40(sp)
  addi sp, sp, 48
  ret
.global main
main:
entry.0:
addi sp, sp, -32
  sw ra, 28(sp)
  sw s0, 24(sp)
  mv s0, sp
  call bump
  sw a0, 0(s0)
  call bump
  sw a0, 4(s0)
  lw t0, 0(s0)
  lw t1, 4(s0)
  add t2, t0, t1
  sw t2, 8(s0)
  lw a0, 8(s0)
  mv sp, s0
  lw ra, 28(sp)
  lw s0, 24(sp)
  addi sp, sp, 32
  ret
