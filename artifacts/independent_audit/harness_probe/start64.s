.section .text
.globl _start
_start:
  call main
  la t0, tohost
  slli t1, a0, 1
  ori t1, t1, 1
  srli t2, a0, 31
  sw t1, 0(t0)
  sw t2, 4(t0)
1: j 1b
.section .tohost,"aw",@progbits
.align 3
tohost: .dword 0
fromhost: .dword 0