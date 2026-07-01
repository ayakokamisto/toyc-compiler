# Java to New C++ Refactor Mapping Spec

## Scope

Route: use `toy-c-compiler-master` as the Java 40-point behavior and optimization reference, then rebuild the next C++ compiler independently.

Current stage:

- Audit the complete Java compile pipeline.
- Map Java behavior, IR, optimization passes, backend register/stack strategy, and RV32 instruction selection to new C++ insertion points.
- Establish a Java / `src2` / new C++ assembly and Spike behavior differential framework.
- Keep `src2` read-only.
- Avoid performance-pass migration in this stage.

Current worktree note: the checked-out tree currently contains `toy-c-compiler-master/` and `src2/`; the prior `src/` tree appears deleted in the working tree before this task started. The new C++ insertion paths below are the target layout for the refactor, not an implementation performed in this stage.

## Java Pipeline Evidence

Authoritative Java entry point:

- `toy-c-compiler-master/src/main/java/toyc/Main.java`

Pipeline order:

1. Parse flags: `-opt`, `--dump-ast`, `--dump-tokens`.
2. Read UTF-8 source from stdin.
3. `LexerFacade.scan(source)` for `--dump-tokens`.
4. `ParserFacade.parse(source)` to produce `Program`.
5. `ASTPrinter.print(ast)` for `--dump-ast`.
6. `SemanticAnalyzer.analyze(ast)` to produce `SemanticResult`.
7. `IRBuilder.build(ast, sem)` to produce `IRProgram`.
8. `Optimizer.optimize(ir)` only when `-opt` is present.
9. `RiscVEmitter.emit(ir, sem)` to produce assembly on stdout.

Failure behavior: `IOException | RuntimeException` prints `toyc: compilation failed: <message>` and exits 1.

## Frontend and Semantic Mapping

| Java reference | Behavior | New C++ insertion point |
| --- | --- | --- |
| `src/main/antlr/ToyC.g4` | ANTLR grammar for ToyC lexical and parser rules. | `src/frontend/lexer.*`, `src/frontend/parser.*`; keep handwritten C++ parser grammar-aligned with Java grammar. |
| `LexerFacade` | Token scan wrapper and frontend diagnostics. | `src/frontend/lexer.*`, `include/toyc/frontend/token.h`. |
| `ParserFacade` | Parser wrapper, syntax error path. | `src/frontend/parser.*`. |
| `ASTBuilder` and `frontend/ast/*` | Java AST node model: program, declarations, functions, statements, expressions. | `include/toyc/frontend/ast.h`, `src/frontend/ast.cpp`. |
| `SemanticAnalyzer` | Scope tree, symbol binding, expression type map, const values, assignment target map, call target map, loop depth checks. | `src/sema/sema.cpp`, `src/sema/semantic_model.*`, `src/sema/const_eval.*`, `src/sema/return_check.*`. |

Semantic requirements to preserve:

- `int main()` must exist and take no parameters.
- Global variable initializers must be compile-time constants.
- Const initializers must be compile-time constants.
- Local and global duplicate declarations are rejected.
- Function names cannot be used as values.
- Variables cannot be called as functions.
- Const assignment is rejected.
- `break` and `continue` require positive loop depth.
- `void` expressions cannot be used as int values, assignment RHS, conditions, or int returns.
- Int functions require value returns along all accepted paths.
- Const folding includes unary, binary arithmetic, comparisons, logical operators, and division/modulo-by-zero diagnostics.

## Java IR Data Structure Mapping

Java IR is in `toy-c-compiler-master/src/main/java/toyc/ir`.

| Java IR | Role | New C++ insertion point |
| --- | --- | --- |
| `IRProgram` | Owns module. | `src/ir/ir.h` or `src/ir/module.*`. |
| `block/Module` | Functions, globals, constants, main function lookup. | `src/ir/ir.h`, `src/ir/contract_ir_generator.*`. |
| `block/Function` | Parameters, locals, basic blocks, entry label. | `src/ir/ir.h`. |
| `block/BasicBlock` | Phi list, instruction list, terminator. | `src/ir/ir.h`, `src/ir/ir_cfg.*`. |
| `value/Constant` | Named or literal int constants. | `src/ir/ir.h`, const operand model. |
| `value/GlobalVar` | Global storage with initial value. | `contract::GlobalObject` equivalent. |
| `value/LocalVar` | Addressable local or parameter. | VReg/symbol mapping in `contract_ir_generator`. |
| `value/Temp` | SSA-like temporary. | VReg string or value id. |
| `value/Label` | Basic-block label. | `IRBlock::label`. |
| `inst/Alloca` | Local stack object declaration before mem2reg. | C++ can model as declaration metadata or initial local vreg. |
| `inst/Load` / `Store` | Addressed memory access for locals/globals. | `LoadGlobalInst`, `StoreGlobalInst`, `CopyInst`, future memory ops if pointer-like IR is kept. |
| `inst/LoadImm` | Constant materialization. | `ConstInst`. |
| `inst/Move` | Copy. | `CopyInst`. |
| `inst/Phi` | SSA merge. | Future SSA IR phi or C++ opt-only phi pass. |
| `inst/BinaryOp` | ADD/SUB/MUL/DIV/MOD. | `AddInst`, `SubInst`, `MulInst`, `DivInst`, `ModInst`. |
| `inst/Compare` | LT/GT/LE/GE/EQ/NE. | `LtInst`, `GtInst`, `LeInst`, `GeInst`, `EqInst`, `NeInst`. |
| `inst/UnaryOp` | NEG/NOT. | `NegInst`, `LNotInst`. |
| `inst/Call` | Function call with optional result. | `CallInst`, `CallVoidInst`. |
| `inst/Branch`, `CondBranch`, `Return` | Terminators. | `BranchInst`, `CondBranchInst`, `ReturnInst`. |

IRBuilder behavior to preserve:

- Emit globals first, then functions.
- Parameters become `LocalVar` values and are allocated at function entry.
- Local var declarations emit `Alloca` plus `Store(init, local)`.
- Local const declarations become `Constant` references.
- `if` emits then/else/end labels and falls through to end through explicit branch.
- `while` emits cond/body/end labels, with cond as continue target and end as break target.
- Logical `&&` and `||` are short-circuit control-flow values.
- Missing int return gets default `0`; missing void return gets void return.

## Java Optimization Pass Order

Authoritative file:

- `toy-c-compiler-master/src/main/java/toyc/opt/Optimizer.java`

Exact Java `-opt` order:

1. For every function:
   - `ControlFlowSimplifier.simplify(function)`
   - `Mem2Reg.promote(function)`
2. Module-wide:
   - `SmallFunctionInliner.inline(program.module())`
   - `FunctionEffects.analyze(program.module())`
3. For every function:
   - `PhiLowerer.lower(function)`
   - `TailRecursionEliminator.eliminate(function)`
   - `GlobalScalarReplacement.replace(function, effects)`
   - Fixed point loop:
     - `LocalValueOptimizer.optimize(function)`
     - `DeadCodeEliminator.eliminate(function, effects)`
     - `LinearExpressionOptimizer.optimize(function)`
     - `LocalCse.eliminate(function)`
     - `LoopInvariantCodeMotion.hoist(function)`
     - `GlobalLoopStorePromotion.promote(function)`
     - `ControlFlowSimplifier.simplify(function)`
     - `DeadStoreEliminator.eliminate(function)`
     - `DeadGlobalStoreEliminator.eliminate(program.module())`
4. Module-wide:
   - `DeadFunctionEliminator.eliminate(program.module())`

## Per-Pass Mapping

| Java pass | Main behavior | New C++ insertion point | Current-stage action |
| --- | --- | --- | --- |
| `ControlFlowSimplifier` | Remove instructions after terminators, fold constant branches, bypass jump-only blocks, merge linear blocks, remove unreachable blocks. | `src/ir/ir_passes.cpp` as `runControlFlowSimplifier`; CFG helpers in `src/ir/ir_cfg.*`. | Specify only. |
| `Mem2Reg` | Promote local alloc/load/store to SSA values and phi nodes using CFG/dominance. | New SSA-capable IR pass, or future `src/ir/ssa/*`. | Specify only. |
| `SmallFunctionInliner` | Inline small safe callees, clone instructions and map values. | `src/ir/ir_passes.cpp` module pass. | Specify only. |
| `FunctionEffects` | Summarize calls/global reads/writes for DCE and global passes. | `src/analysis/function_effects.*`. | Specify only. |
| `PhiLowerer` | Lower phi into edge/block copies before backend. | `src/ir/ir_passes.cpp` or backend pre-lowering pass. | Specify only. |
| `TailRecursionEliminator` | Rewrite self-tail call to loop. | `src/ir/ir_passes.cpp`, gated by O3. | Specify only. |
| `GlobalScalarReplacement` | Replace eligible global loads/stores using scalar temporaries. | `src/ir/ir_passes.cpp` plus effect analysis. | Specify only. |
| `LocalValueOptimizer` | Local constant/algebraic simplifications and dead temp cleanup. | `src/ir/ir_passes.cpp`. | Specify only. |
| `DeadCodeEliminator` | Remove pure dead instructions while preserving calls/stores/terminators/effectful ops. | `src/ir/ir_passes.cpp`. | Specify only. |
| `LinearExpressionOptimizer` | Combine simple linear expressions. | `src/ir/ir_passes.cpp`. | Specify only. |
| `LocalCse` | Block-local common subexpression elimination. | `src/ir/ir_passes.cpp`. | Specify only. |
| `LoopInvariantCodeMotion` | Hoist safe invariant instructions out of loops. | `src/ir/ir_passes.cpp`, CFG/loop analysis. | Specify only. |
| `GlobalLoopStorePromotion` | Promote eligible global loop stores. | `src/ir/ir_passes.cpp` plus alias/effect limits. | Specify only. |
| `DeadStoreEliminator` | Remove overwritten unused local stores. | `src/ir/ir_passes.cpp`. | Specify only. |
| `DeadGlobalStoreEliminator` | Remove dead global stores using module/effect knowledge. | `src/ir/ir_passes.cpp`. | Specify only. |
| `DeadFunctionEliminator` | Keep reachable functions from main and calls. | `src/ir/ir_passes.cpp` module pass. | Specify only. |

## New C++ Pass Order Target

Target order for a future C++ optimizer matching Java behavior:

```text
for function:
  simplify_cfg
  mem2reg
module:
  small_function_inline
  function_effects
for function:
  phi_lower
  tail_recursion_eliminate
  global_scalar_replacement
  do:
    local_value_opt
    dce
    linear_expression_opt
    local_cse
    licm
    global_loop_store_promotion
    simplify_cfg
    dead_store_eliminate
    dead_global_store_eliminate(module)
  while changed
module:
  dead_function_eliminate
```

`src2` currently uses a smaller iterative set in `src2/ir/ir_optimizer.cpp`: copy propagation, const propagation, local CSE, DCE, O3 tail recursion, O3 LICM, and final DCE. It is useful as a C++ structural reference, while Java remains the broader 40-point optimization behavior reference.

## Backend Register and Stack Strategy

Authoritative Java backend:

- `toy-c-compiler-master/src/main/java/toyc/backend/RiscVEmitter.java`

Register policy:

- Argument registers: `a0-a7`.
- Always considered allocatable first: `s1-s11`.
- Extra allocatable registers for leaf functions: `t3`, `t4`, `t5`, `a2-a7`.
- Allocation threshold: score >= 6.
- Scoring weights loop-like blocks by 12 and other blocks by 1.
- Adjacent producer -> `Move` pairs are unioned before scoring.
- Values in the same union root receive the same allocated register.
- Calls make a function non-leaf; leaf extra registers are unavailable when any call appears.

Stack/frame policy:

- Frame pointer is `s0`.
- Prologue:
  - adjust `sp` by negative aligned frame size
  - save `ra` at `frameSize - 4`
  - save `s0` at `frameSize - 8`
  - save used allocated `s*` registers at descending offsets
  - `mv s0, sp`
  - land incoming parameters
- Epilogue:
  - `mv sp, s0`
  - restore `ra`, `s0`, saved allocated registers
  - adjust `sp` back by frame size
  - `ret`
- Local and temp stack slots use offsets from `s0`.
- Stack arguments beyond `a0-a7` are passed on a transient call stack area aligned to 16 bytes.
- Incoming stack arguments are loaded from caller frame using `frameSize + (i - 8) * 4` relative to `s0`.

New C++ insertion points:

- `src/codegen/frame/RegisterAllocator.*`: score-based Java-compatible allocator.
- `src/codegen/frame/StackFrame.*`: Java-compatible `s0` anchored frame model.
- `src/codegen/abi/CallingConvention.*`: prologue, epilogue, param landing, call args.
- `src/codegen/lower/InstructionSelector.*`: load/store/value movement and instruction selection.

## RV32I/RV32IM Instruction Selection Mapping

Java backend emits RV32IM when multiplication/division/remainder are left as hardware ops:

| Java IR op | Java emission | New C++ insertion point |
| --- | --- | --- |
| `BinaryOp.ADD` | `add`, or `addi` for signed 12-bit immediate. | `InstructionSelector::emitBinaryOp`. |
| `BinaryOp.SUB` | `sub`, or `addi` with negated immediate. | `InstructionSelector::emitBinaryOp`. |
| `BinaryOp.MUL` | `mul`; constant power-of-two multiply becomes `slli` plus optional `neg`. | `InstructionSelector::emitBinaryOp`; constant strength reduction pass or selector fold. |
| `BinaryOp.DIV` | `div`; non-negative divide by power-of-two becomes `srli`; non-negative divide by positive constant uses magic multiply with `mulhu`. | Selector fold plus non-negative analysis. |
| `BinaryOp.MOD` | `rem`; non-negative modulo by power-of-two becomes mask; non-negative modulo by positive constant uses magic divide then multiply/subtract. | Selector fold plus non-negative analysis. |
| `Compare.LT` | `slt`; immediate form uses `slti`. | Compare lowering. |
| `Compare.GT` | `slt` with operands swapped. | Compare lowering. |
| `Compare.LE` | `slt` swapped plus `xori 1`, or `slti imm+1`. | Compare lowering. |
| `Compare.GE` | `slt` plus `xori 1`, or `slti imm` plus `xori 1`. | Compare lowering. |
| `Compare.EQ` | subtract/add-immediate then `seqz`. | Compare lowering. |
| `Compare.NE` | subtract/add-immediate then `snez`. | Compare lowering. |
| Unary neg | `sub target, zero, src`. | Unary lowering. |
| Unary not | `seqz`. | Unary lowering. |
| Branch | `j`, `bnez`, direct compare branch fusion (`beq`, `bne`, `blt`, `bge`, zero branch pseudo forms). | Terminator lowering and branch fusion. |
| Global load/store | `la`, `lw`, `sw`. | Global instruction lowering. |
| Call | move/load args to `a0-a7`, spill extra args to call stack, `call`, cleanup, move `a0` result. | CallingConvention plus selector call lowering. |

Java backend peephole:

- Removes identity `mv x, x`.

`src2` backend reference:

- `src2/codegen/RiscvBackend.cpp`: module emission and backend peephole gate.
- `src2/codegen/lower/FunctionEmitter.cpp`: function prologue/body/epilogue sequencing.
- `src2/codegen/lower/InstructionSelector.cpp`: instruction and terminator lowering.
- `src2/codegen/abi/CallingConvention.cpp`: frame and calling convention handling.
- `src2/codegen/frame/RegisterAllocator.cpp`: allocation model.
- `src2/codegen/opt/PeepholeOptimizer.cpp`: C++ peephole reference.

## Differential Framework

New framework:

- `tools/triple_diff/README.md`
- `tools/triple_diff/run.ps1`
- `tools/triple_diff/run_triple_diff.sh`

Inputs:

- Java compiler root: `toy-c-compiler-master`
- `src2` root: `src2`
- New C++ compiler path: configurable; default `build/toycc.exe`
- Cases: defaults to Java smoke and bench `.tc` resources

Outputs:

- Per-compiler assembly files
- ELF files when linking succeeds
- Spike logs
- opcode counts for `lw`, `sw`, `jal`, `call`, `ret`, `mul`, `div`, `rem`
- signed return value parsed from `tohost`
- markdown report

Use this framework before each future migration step. A Java pass is ready for C++ reimplementation only when the framework shows the intended behavior delta and the C++ side has a targeted test for the pass.
