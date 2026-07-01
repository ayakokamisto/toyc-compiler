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
  j if.then.1
if.then.1:
  j if.else.4
if.end.2:
  lw t0, 16(s0)
  sw t0, 4(s0)
  lw a0, 4(s0)
  mv sp, s0
  lw ra, 28(sp)
  lw s0, 24(sp)
  addi sp, sp, 32
  ret
if.then.3:
  li t0, 1
  sw t0, 8(s0)
  lw t0, 8(s0)
  sw t0, 16(s0)
  j if.end.5
if.else.4:
  li t0, 2
  sw t0, 12(s0)
  lw t0, 12(s0)
  sw t0, 16(s0)
  j if.end.5
if.end.5:
  j if.end.2
