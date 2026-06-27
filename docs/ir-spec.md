# IR Specification

## P6 SSA Form

`IRForm::CanonicalSlot` is the backend input form with `load.slot` and `store.slot`. `IRForm::SSA` is produced by Mem2Reg and uses Phi instructions at block prefixes.

Phi format:

```text
%v7 = phi [entry: %v2], [if.then.0: %v5]
```

SSA verification checks unique definitions, Phi incoming predecessor coverage, use dominance, and absence of promoted slot accesses.

## Overview

ToyC uses a two-phase IR:

1. **Canonical Slot IR** — mutable variables as slots, easy to generate from AST.
2. **SSA IR** — after Mem2Reg, slots are promoted to SSA values with phi nodes.

## Canonical Slot IR (P4 — implemented)

### Slot Operations

| Instruction | Operands | Description |
|-------------|----------|-------------|
| `load.slot` | `SlotId` | Load value from a mutable slot. |
| `store.slot` | `SlotId`, `Value` | Store value to a mutable slot. |
| `load.global` | `GlobalId` | Load value from a global variable. |
| `store.global` | `GlobalId`, `Value` | Store value to a global variable. |

### Arithmetic / Logic

| Instruction | Operands | Description |
|-------------|----------|-------------|
| `unary` | `UnaryOp`, `Value` | Unary operation (neg, not). |
| `binary` | `BinaryOp`, `Value`, `Value` | Binary operation (add, sub, mul, div, mod). |
| `cmp` | `CmpPred`, `Value`, `Value` | Comparison (eq, ne, lt, le, gt, ge). Result is 0 or 1. |

### Control Flow

| Instruction | Operands | Description |
|-------------|----------|-------------|
| `br` | `BlockId` | Unconditional branch. |
| `condbr` | `Value`, `BlockId`, `BlockId` | Conditional branch (non-zero → true, zero → false). |
| `ret` | `Value?` | Return (optional value for int functions). |
| `call` | `FunctionId`, `Value*` | Function call. |

### Slot Kinds

| Kind | Description |
|------|-------------|
| `Parameter` | Created for a function parameter. |
| `LocalVariable` | Created for a local `var` declaration. |
| `Temporary` | Created for short-circuit boolean materialization. |

### Global Kinds

| Kind | Description |
|------|-------------|
| `Variable` | Mutable global variable. |
| `Constant` | Compile-time constant (directly materialized). |
| `InternalGuard` | Internal guard for runtime initialization. |

## SSA IR

After Mem2Reg:

- `slot.load` / `slot.store` are eliminated (replaced by SSA values).
- `phi` instructions are inserted at CFG join points.
- Every `Value` has a unique `ValueId`.
- Def-use chains are maintained via intrusive `Use` linked lists.

### Phi Instruction

```
%3 = phi [%1, %block0], [%2, %block1]
```

- Each incoming edge pairs a value with its source block.
- Ensures each variable is defined exactly once on every path.

## Type System

ToyC IR types are minimal:

| Type | Description |
|------|-------------|
| `i32` | 32-bit signed integer. |
| `void` | No value (function return type). |
| `label` | Basic block label (for branch targets). |
