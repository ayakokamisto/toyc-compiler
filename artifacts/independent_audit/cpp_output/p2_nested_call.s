.text
.global id
id:
entry.0:
addi sp, sp, -16
  sw ra, 12(sp)
  sw s0, 8(sp)
  mv s0, sp
  sw a0, 4(s0)
  
  lw t0, 4(s0)
  sw t0, 0(s0)
  lw a0, 0(s0)
  mv sp, s0
  lw ra, 12(sp)
  lw s0, 8(sp)
  addi sp, sp, 16
  ret
.global add
add:
entry.0:
addi sp, sp, -32
  sw ra, 28(sp)
  sw s0, 24(sp)
  mv s0, sp
  sw a0, 12(s0)
  sw a1, 16(s0)
  
  
  lw t0, 12(s0)
  sw t0, 0(s0)
  lw t0, 16(s0)
  sw t0, 4(s0)
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
.global main
main:
entry.0:
addi sp, sp, -32
  sw ra, 28(sp)
  sw s0, 24(sp)
  mv s0, sp
  li t0, 17
  sw t0, 0(s0)
  lw a0, 0(s0)
  call id
  sw a0, 4(s0)
  li t0, 25
  sw t0, 8(s0)
  lw a0, 8(s0)
  call id
  sw a0, 12(s0)
  lw a0, 4(s0)
  lw a1, 12(s0)
  call add
  sw a0, 16(s0)
  lw a0, 16(s0)
  mv sp, s0
  lw ra, 28(sp)
  lw s0, 24(sp)
  addi sp, sp, 32
  ret
