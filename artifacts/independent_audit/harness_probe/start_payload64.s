.section .text
.globl _start
_start:
  call main
  la t0, tohost
  li t1, 2
  sw t1, 0(t0)
  sw a0, 4(t0)
1: j 1b
.section .tohost,"aw",@progbits
.align 3
tohost: .dword 0
fromhost: .dword 0