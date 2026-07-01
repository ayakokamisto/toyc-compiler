.text
.global fib
fib:
entry.0:
addi sp, sp, -80
  sw ra, 76(sp)
  sw s0, 72(sp)
  mv s0, sp
  sw a0, 56(s0)
  
  lw t0, 56(s0)
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
  lw t0, 56(s0)
  sw t0, 16(s0)
  lw a0, 16(s0)
  mv sp, s0
  lw ra, 76(sp)
  lw s0, 72(sp)
  addi sp, sp, 80
  ret
if.end.2:
  lw t0, 56(s0)
  sw t0, 20(s0)
  li t0, 1
  sw t0, 24(s0)
  lw t0, 20(s0)
  lw t1, 24(s0)
  sub t2, t0, t1
  sw t2, 28(s0)
  lw a0, 28(s0)
  call fib
  sw a0, 32(s0)
  lw t0, 56(s0)
  sw t0, 36(s0)
  li t0, 2
  sw t0, 40(s0)
  lw t0, 36(s0)
  lw t1, 40(s0)
  sub t2, t0, t1
  sw t2, 44(s0)
  lw a0, 44(s0)
  call fib
  sw a0, 48(s0)
  lw t0, 32(s0)
  lw t1, 48(s0)
  add t2, t0, t1
  sw t2, 52(s0)
  lw a0, 52(s0)
  mv sp, s0
  lw ra, 76(sp)
  lw s0, 72(sp)
  addi sp, sp, 80
  ret
.global main
main:
entry.0:
addi sp, sp, -16
  sw ra, 12(sp)
  sw s0, 8(sp)
  mv s0, sp
  li t0, 8
  sw t0, 0(s0)
  lw a0, 0(s0)
  call fib
  sw a0, 4(s0)
  lw a0, 4(s0)
  mv sp, s0
  lw ra, 12(sp)
  lw s0, 8(sp)
  addi sp, sp, 16
  ret
