.section .text
.globl _start
_start:
  call main
  la t0, tohost
  srli t2, a0, 31
  slli t1, t2, 2
  ori t1, t1, 2
  slli t3, a0, 1
  srli t3, t3, 1
  sw t1, 0(t0)
  sw t3, 4(t0)
1: j 1b
.section .tohost,"aw",@progbits
.align 3
tohost: .dword 0
fromhost: .dword 0