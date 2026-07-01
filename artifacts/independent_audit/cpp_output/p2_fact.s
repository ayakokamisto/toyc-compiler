.text
.global fact
fact:
entry.0:
addi sp, sp, -64
  sw ra, 60(sp)
  sw s0, 56(sp)
  mv s0, sp
  sw a0, 44(s0)
  
  lw t0, 44(s0)
  sw t0, 0(s0)
  li t0, 1
  sw t0, 4(s0)
  lw t0, 0(s0)
  lw t1, 4(s0)
  slt t2, t1, t0
  xori t2, t2, 1
  sw t2, 8(s0)
  lw t0, 8(s0)
  li t1, 0
  xor t2, t0, t1
  snez t2, t2
  sw t2, 12(s0)
  lw t0, 12(s0)
  bnez t0, if.then.1
  j if.end.2
if.then.1:
  li t0, 1
  sw t0, 16(s0)
  lw a0, 16(s0)
  mv sp, s0
  lw ra, 60(sp)
  lw s0, 56(sp)
  addi sp, sp, 64
  ret
if.end.2:
  lw t0, 44(s0)
  sw t0, 20(s0)
  lw t0, 44(s0)
  sw t0, 24(s0)
  li t0, 1
  sw t0, 28(s0)
  lw t0, 24(s0)
  lw t1, 28(s0)
  sub t2, t0, t1
  sw t2, 32(s0)
  lw a0, 32(s0)
  call fact
  sw a0, 36(s0)
  lw t0, 20(s0)
  lw t1, 36(s0)
  mul t2, t0, t1
  sw t2, 40(s0)
  lw a0, 40(s0)
  mv sp, s0
  lw ra, 60(sp)
  lw s0, 56(sp)
  addi sp, sp, 64
  ret
.global main
main:
entry.0:
addi sp, sp, -16
  sw ra, 12(sp)
  sw s0, 8(sp)
  mv s0, sp
  li t0, 5
  sw t0, 0(s0)
  lw a0, 0(s0)
  call fact
  sw a0, 4(s0)
  lw a0, 4(s0)
  mv sp, s0
  lw ra, 12(sp)
  lw s0, 8(sp)
  addi sp, sp, 16
  ret
