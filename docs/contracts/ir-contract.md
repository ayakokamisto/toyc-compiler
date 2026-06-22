# IR and CFG Contract

## Public Model

The IR data model lives in `src/ir/ir.h` under `toyc::ir`:

```text
Module
|- Global
`- Function
   |- Parameter
   `- BasicBlock
      `- Instruction
```

IR types are `I32` and `Void`. A `ValueId` is unique within one function. A `BlockId`
identifies a basic block within one function.

An `Operand` is an `Immediate`, function-local `Value`, or `GlobalRef`. Calls store the
callee name directly in `CallInst::callee`.

## Instructions

- `BinaryInst`: addition, subtraction, multiplication, division, and remainder.
- `CompareInst`: `<`, `>`, `<=`, `>=`, `==`, and `!=`, producing normalized `i32` 0 or 1.
- `UnaryNotInst`: produces normalized `i32` 0 or 1.
- `AllocaInst`, `LoadInst`, and `StoreInst`: local slots and global accesses.
- `CallInst`: calls with `i32` result IDs or an empty result for `void` calls.
- `JumpInst`, `BranchInst`, and `ReturnInst`: terminators.

Every basic block ends with exactly one terminator. A terminator is the final instruction
in its block. `verifyModule()` checks this invariant, valid branch targets, local ID use,
return compatibility, and other structural constraints before CodeGen.

## Globals and Functions

`Parameter::index` defines source-order parameter position. Function-local `ValueId`
allocation includes instruction results and local slots. `Global::initializer` is an
`int32_t`; Sema guarantees that each global initializer has already evaluated to a
compile-time integer.

## CFG Lowering

- An `if` creates condition, then, optional else, and merge blocks.
- A `while` creates condition, body, and exit blocks. The body back edge targets the
  condition block.
- `break` targets the nearest loop exit block.
- `continue` targets the nearest loop condition block.
- `&&` evaluates its right operand only from the left-true edge.
- `||` evaluates its right operand only from the left-false edge.
- A logical expression used as a value stores 0 or 1 in a temporary stack slot along its
  true and false paths, then loads the value in the merge block. The initial IR omits Phi
  instructions.

## Phase Boundary

`IRGenerator` consumes a Sema-approved `ast::CompUnit` and `sema::SemanticModel`.
CodeGen consumes an IR `Module` accepted by `verifyModule()`. IR generation and CodeGen
leave the AST unchanged.

The `-opt` implementation order is constant folding, dead-code elimination, basic-block
simplification, local common-subexpression elimination, and simple register-allocation
improvements.
