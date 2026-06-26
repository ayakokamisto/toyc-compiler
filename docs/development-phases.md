# Development Phases

## P0: Project Scaffold ✅

- Directory structure, CMake build system, module boundaries.
- Strongly-typed IDs, source locations, diagnostics infrastructure.
- Placeholder headers for all modules (Lexer, Parser, AST, Sema, IR, Analysis, Passes, MIR, RISC-V32).
- Smoke tests for Token, CompilerOptions, and basic IR types.
- Justfile, README, documentation.

## P1: Token + Lexer + Token Dump

- Implement `Lexer::tokenize()` — real scanning.
- Cover all ToyC tokens from `词汇翻译表.md`.
- Add `--dump-tokens` flag to `toycc` for debugging.
- Comprehensive lexer tests with fixture files.

## P2: Parser + AST ✅

- Define full AST node hierarchy.
- Implement recursive descent parser with full precedence climbing.
- AST pretty-printer for debugging (`--dump-ast`).
- Parser error recovery (no infinite loops).
- Comprehensive parser tests (193 frontend tests passing).

## P3: Sema + Constant Evaluation

- Symbol table with scopes (block nesting).
- Type checking (int vs void, function signatures).
- Constant folding — `const` must be evaluable at compile time.
- Short-circuit evaluation for `&&` and `||`.
- All `int` functions must return on every path.
- `return -2147483648` boundary handling.

## P4: Slot IR + CFG

- AST → Canonical Slot IR generation.
- `slot.load` / `slot.store` for local variables.
- `global.load` / `global.store` for globals.
- CFG construction (predecessor/successor edges).
- IR printer and verifier.

## P5: O0 RV32 Backend

- Direct slot IR → RISC-V32 assembly (no optimization).
- Stack-based code generation (load/store everything).
- Frame layout, prologue/epilogue.
- `ecall` for program exit with `a0` return value.
- Pass all functional tests at O0 level.

## P6: Mem2Reg + SSA Verifier

- Promote mutable slots to SSA values.
- Insert phi nodes at dominance frontiers.
- Dominator tree computation.
- SSA structural verifier.
- Def-use chain maintenance.

## P7: Basic Optimization

- Dead code elimination (DCE).
- Constant folding in IR.
- Copy propagation.
- Simple peephole optimizations.

## P8: GVN / LICM / Register Allocation

- Global Value Numbering (GVN).
- Loop-Invariant Code Motion (LICM) — requires LoopInfo.
- Out-of-SSA (phi elimination, copy insertion).
- Liveness analysis.
- Linear scan register allocation.
- Frame layout finalization.

## P9: Differential Testing, Performance, Report

- Differential testing: compare `toycc` output with `gcc` output on same input.
- Performance testing with `-opt` flag.
- Benchmarking against `gcc -O2` baseline.
- Practice report writing.
