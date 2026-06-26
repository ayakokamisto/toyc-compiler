# Backend Specification

## Target: RISC-V32

- 32-bit RISC-V (RV32I base integer ISA).
- All values are 32-bit signed integers (`int`).
- `main` return value goes into `a0`, exited via `ecall`.

## Register Conventions

| Register | ABI Name | Role |
|----------|----------|------|
| x0 | zero | Hardwired zero |
| x1 | ra | Return address |
| x2 | sp | Stack pointer |
| x8 | s0/fp | Frame pointer / saved |
| x10-x11 | a0-a1 | Arguments / return values |
| x12-x17 | a2-a7 | Arguments |
| x5-x7 | t0-t2 | Temporaries (caller-saved) |
| x18-x27 | s2-s11 | Saved (callee-saved) |
| x28-x31 | t3-t6 | Temporaries (caller-saved) |

## Calling Convention

- First 8 arguments in `a0`–`a7`.
- Return value in `a0`.
- Caller saves `t0`–`t6`, `a0`–`a7`, `ra`.
- Callee saves `s0`–`s11`.
- Stack aligned to 16 bytes.

## Assembly Emission

Output is standard RISC-V assembly text:

```asm
  .text
  .globl main
main:
  addi sp, sp, -16
  sw ra, 12(sp)
  # ... body ...
  lw ra, 12(sp)
  addi sp, sp, 16
  ret
```

## Frame Layout

```
┌─────────────────┐  ← high address
│   arguments     │
├─────────────────┤
│   return addr   │
├─────────────────┤
│   saved regs    │
├─────────────────┤
│   locals        │
├─────────────────┤
│   spills        │
├─────────────────┤  ← sp (aligned to 16)
└─────────────────┘
```

## Lowering Pipeline

1. SSA IR → Out-of-SSA (phi elimination, copy insertion)
2. Out-of-SSA → MIR (virtual registers, target opcodes)
3. MIR → Liveness analysis
4. Liveness → Register allocation (linear scan)
5. Register allocation → Frame layout
6. Frame layout → Assembly emission
