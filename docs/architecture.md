# Architecture

## P6 / P7A Pipeline Boundary

```text
Canonical Slot IR
-> DominatorTree / Dominance Frontier
-> Mem2Reg
-> SSA IR
-> SSA Verifier
-> --dump-ssa
```

Normal code generation follows `Canonical Slot IR -> P5 MIR -> Spill-All -> Assembly`.

Optimized `toycc -opt` code generation follows
`Canonical Slot IR -> removeUnreachableBlocks -> Mem2Reg -> SSA Verifier ->
InstCombineLite / SCCP / SimplifyCFG / DCE fixed point -> SSA Verifier ->
Out-of-SSA / Phi lowering -> Canonical Slot IR Verifier -> P5 MIR -> Spill-All -> Assembly`.

## Module Dependency Direction

```
toyc_support
  ↑
  ├── toyc_frontend  (Lexer / Parser / AST)
  ├── toyc_sema      (Semantic analysis)
  ├── toyc_ir        (Canonical Slot IR — Builder / Printer / Verifier)
  │     ↑
  │     ├── toyc_analysis   (CFG construction)
  │     ├── toyc_lowering   (AST + Sema → IR)
  │     └── toyc_passes     (Optimization passes, future)
  ├── toyc_mir       (Machine IR, future)
  │     ↑
  │     └── toyc_riscv32    (RISC-V32 backend, future)
  └── toycc          (Driver / main)
```

All modules depend on `toyc_support` (common types, IDs, diagnostics).
No circular dependencies.

## Compilation Pipeline

```
Source (stdin)
→ Lexer → Token stream
→ Parser → AST (CompUnit root)
→ SemanticModel → Symbol tables, type info, const folding
→ Canonical Slot IR → Mutable slots (slot.load / slot.store)
→ Mem2Reg → SSA IR (phi nodes, def-use chains)
→ Optimizing SSA IR → (GVN, LICM, DCE, etc.)
→ Out-of-SSA → MIR with virtual registers
→ RV32 MIR → Target-specific instructions
→ Register Allocation / Frame Layout → Physical registers, stack slots
→ RISC-V32 Assembly (stdout)
```

## Three IR Layers

### 1. Canonical Slot IR

- Mutable local variables are represented as **slots** (`SlotId`).
- Global variables as **globals** (`GlobalId`).
- Operations: `slot.load`, `slot.store`, `global.load`, `global.store`, `unary`, `binary`, `cmp`, `call`, `br`, `condbr`, `ret`.
- This is the initial IR generated from AST — easy to construct, no SSA constraints.

### 2. Optimizing SSA IR

- After Mem2Reg, mutable slots are promoted to SSA values.
- Each `Value` has a unique `ValueId`; current passes rebuild use information by scanning operands.
- `Phi` nodes merge values at CFG join points.
- `BasicBlock` → `Function` → `Module` hierarchy.
- P7A optimization passes operate on this IR and lower back to Canonical Slot IR before P5.

### 3. MIR (Machine IR)

- Lowered to target-specific virtual registers (`VRegId`).
- Operands: virtual registers, physical registers, immediates, stack slots, global symbols, block labels.
- Register allocation maps virtual → physical registers.
- Frame layout computes stack offsets.

## ID Responsibilities

| ID | Domain | Purpose |
|----|--------|---------|
| `ValueId` | IR | Unique identifier for every computed value (instruction results, function arguments). |
| `SlotId` | Slot IR | Identifier for mutable local variable slots (pre-Mem2Reg). |
| `SymbolId` | Sema | Symbol table entry — tracks declarations, types, scopes. |
| `BlockId` | IR | Basic block identifier — used in CFG edges and branch targets. |
| `FunctionId` | IR | Function identifier — used in call instructions and module lookup. |
| `GlobalId` | IR | Global variable / constant identifier. |
| `VRegId` | MIR | Virtual register identifier — used before register allocation. |
| `InstId` | IR | Instruction identifier (for passes that need to reference instructions). |
| `SourceId` | Frontend | Source file identifier (for multi-file support, future). |

## Operator Precedence Note

The ToyC grammar in `任务要求.md` uses a compressed notation where `RelExpr` includes
all comparison operators (`< > <= >= == !=`) at the same precedence level. However,
the **semantic constraints** section states that "运算符的优先级、结合性和计算规则等与 C 语言一致"
(operator precedence, associativity, and evaluation rules are consistent with C).

Therefore, the parser implements C-standard precedence where relational operators
(`< <= > >=`) bind tighter than equality operators (`== !=`):

```
2 == 3 < 4  →  2 == (3 < 4)  →  Equal(2, Less(3, 4))
```

This decision is validated by parser tests including
`RelationEqualPrecedence_2_eq_3_lt_4`.

## Implementation Order

1. **P0**: Project scaffold (this phase)
2. **P1**: Token + Lexer + token dump
3. **P2**: Parser + AST
4. **P3**: Sema + constant evaluation
5. **P4**: Slot IR + CFG construction ✅
6. **P5**: O0 RV32 backend (no optimization)
7. **P6**: Mem2Reg + SSA verifier
8. **P7**: Basic optimization passes (DCE, constant folding in IR)
9. **P8**: GVN / LICM / Register Allocation
10. **P9**: Differential testing, performance testing, report
