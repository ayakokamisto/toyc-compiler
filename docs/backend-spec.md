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

## P5 ABI Contract (O0 RV32 Backend)

P5 implements a conservative, stack-based RV32 backend with no optimization.

### Function ABI

1. All ToyC functions follow RV32 integer function ABI conventions.
2. The first 8 `int` parameters are passed in `a0`–`a7`.
3. Additional parameters (if any) are passed via the caller's stack area.
4. `int` return values are placed in `a0`.
5. `void` functions do not set a return value.
6. Both ordinary functions and `main` return to the caller via `ret`.
7. `main` does not directly execute `ecall`; the C runtime wrapper handles exit.
8. Call sites observe caller-saved (`t0`–`t6`, `a0`–`a7`, `ra`) and callee-saved (`s0`–`s11`) conventions.
9. Every call site maintains 16-byte stack alignment.
10. P5 uses conservative stack-slot allocation for all values;
    P8 replaces this with real register allocation.

### ISA Decision Table

| Extension | Status | Notes |
|-----------|--------|-------|
| RV32I | Required | Base integer ISA, always available. |
| M extension (mul/div/rem) | Conditional | Use only if local toolchain and OJ environment confirm support. |
| M extension fallback | P5+ | If unavailable, use internal helper calls or equivalent sequences. |

### P5 Code Generation Strategy

- Every IR value is spilled to a stack slot (no register allocation).
- Each `SlotId` maps to a fixed stack offset.
- Each `ValueId` (instruction result) maps to a temporary stack slot.
- `ConstInt` → `li` + store to stack.
- `Binary(Add, a, b)` → load a, load b, `add`, store result.
- `Call` → load arguments into `a0`–`a7` (or stack), `jal`, store `a0` result.
- `CondBr` → load condition, `bnez`/`beqz` to targets.
- `Return` → load value into `a0`, restore `ra`, `ret`.

### Assembly Output Contract

- Output to stdout, one instruction per line.
- `.text` section, `.globl main` directive.
- Function labels match IR function names.
- No data section in P5 (globals handled in future phases).
