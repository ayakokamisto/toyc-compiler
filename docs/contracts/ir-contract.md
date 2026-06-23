# IR and Backend Boundary Contract

## Current Delivery Model

Member three delivers backend-ready IR as `toyc::codegen::contract::IRModule`.
The generator entry point is:

```cpp
toyc::ir::ContractIRGenerator::generate(
    const ast::CompUnit& unit,
    const sema::SemanticModel& semanticModel);
```

The concrete backend consumption model lives in `src/codegen/ContractIR.h`.
The detailed member-three/member-four interface is documented in
`docs/contracts/ir_backend_contract.md`.

`src/ir/ir.h` still describes an older `ValueId`/`BlockId` structural IR model.
It is not the current RISC-V backend input. New member-three work should target
`ContractIRGenerator` unless the team explicitly decides to reintroduce a
separate canonical IR plus adapter.

## Pipeline Boundary

The supported member-three boundary is:

```text
Parser AST + sema::SemanticModel
  -> ir::ContractIRGenerator
  -> ir::verifyContractModule
  -> codegen::RiscvBackend
```

`ContractIRGenerator` expects Parser and Sema to have completed successfully.
It leaves the AST and `SemanticModel` unchanged.

## Generated IR Shape

Generated IR uses non-SSA virtual registers:

- Parameters are named `%p0`, `%p1`, and so on in source order.
- Local variables use stable source-derived vregs such as `%x`; shadowed locals
  receive suffixes such as `%x_1`.
- Expression temporaries use `%t0`, `%t1`, and so on.
- Global variables use `@name`.

The generator emits the instruction set consumed by member four:

- `CONST` and `COPY` for immediates and local value movement.
- `LOAD_GLOBAL` and `STORE_GLOBAL` for global variable access.
- Arithmetic, comparison, unary minus, and logical-not instructions.
- `CALL` for `int` functions and `CALL_VOID` for `void` functions.
- `JUMP`, `BRANCH`, and `RETURN` terminators.

Debug-only contract tables from `ir_backend_contract.md`, such as `constTable`,
`funcTable`, and `symTable`, are not materialized in `ContractIR.h`; the backend
does not consume them.

## CFG Lowering

- Every function starts with an `entry` block.
- Every block has exactly one terminator.
- `if` emits then, optional else, and merge blocks.
- `while` emits condition, body, and exit blocks.
- `break` jumps to the nearest loop exit block.
- `continue` jumps to the nearest loop condition block.
- `&&` and `||` are lowered to explicit short-circuit `BRANCH` CFG. The right
  operand is emitted only inside the path where it may execute.

## Constants And Globals

Global constants and local constants are inlined as `CONST` values. They do not
appear in `IRModule.globalVars`.

Global variables appear in `IRModule.globalVars` with a static `initValue`.
`ContractIRGenerator` evaluates global variable initializers as compile-time
constant expressions and reports diagnostics if that boundary is violated.

## Verification

`verifyContractModule()` checks the member-three/member-four handoff:

- each function has an `entry` block;
- block labels are unique;
- jump and branch targets exist;
- all blocks are reachable from `entry`;
- return operands match the function return type;
- global references name declared global objects;
- vreg uses have a definition source from parameters or instruction results.

The verifier is intentionally structural. Parser and Sema remain responsible for
source-language checks such as declaration-before-use, constant assignment
errors, call arity, loop-control legality, and return completeness.

## Optimization Boundary

The `-opt` flag is consumed by the backend through `BackendOptions`. Member three
emits the same contract IR regardless of optimization mode.
