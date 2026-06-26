# IR Specification

## Overview

ToyC uses a two-phase IR:

1. **Canonical Slot IR** — mutable variables as slots, easy to generate from AST.
2. **SSA IR** — after Mem2Reg, slots are promoted to SSA values with phi nodes.

## Canonical Slot IR

### Slot Operations

| Instruction | Operands | Description |
|-------------|----------|-------------|
| `slot.load` | `SlotId` | Load value from a mutable slot. |
| `slot.store` | `SlotId`, `Value` | Store value to a mutable slot. |
| `global.load` | `GlobalId` | Load value from a global variable. |
| `global.store` | `GlobalId`, `Value` | Store value to a global variable. |

### Arithmetic / Logic

| Instruction | Operands | Description |
|-------------|----------|-------------|
| `unary` | `UnaryOp`, `Value` | Unary operation (neg, not). |
| `binary` | `BinaryOp`, `Value`, `Value` | Binary operation (add, sub, mul, div, mod, and, or). |
| `cmp` | `CmpPred`, `Value`, `Value` | Comparison (eq, ne, lt, le, gt, ge). |

### Control Flow

| Instruction | Operands | Description |
|-------------|----------|-------------|
| `br` | `BlockId` | Unconditional branch. |
| `condbr` | `Value`, `BlockId`, `BlockId` | Conditional branch. |
| `ret` | `Value?` | Return (optional value for int functions). |
| `call` | `FunctionId`, `Value*` | Function call. |

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
