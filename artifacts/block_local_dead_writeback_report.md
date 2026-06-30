# Block-Local Dead Writeback Report

## Scope

This change implements the first controlled backend performance optimization for `E:\TOYC\src`: block-local dead writeback suppression.

The implementation does not change ABI handling, register allocation, frame layout, callee-saved save/restore, caller-saved policy, or cross-block cache behavior.

## Pass Rules

Implementation files:

- `include/toyc/mir/mir_dead_writeback.h`
- `src/mir/mir_dead_writeback.cpp`

The pass runs after MIR generation and dead-vreg cleanup, before register allocation and assembly emission.

The pass has two conservative effects:

1. It deletes a `StoreFrame` when a later `StoreFrame` in the same basic block overwrites the same known local frame slot before any hard boundary.
2. It marks a VReg definition with `suppressVRegHomeStore` when every use of that VReg is in the same basic block segment before any hard boundary. The emitter then skips only that VRegHome `sw` when the VReg also has a physical register assignment.

## Conservative Boundaries

The pass clears same-block knowledge at:

- `LoadFrame`
- `LoadGlobal`
- `StoreGlobal`
- `La`
- `Call`
- `Branch`
- `BranchIfNonZero`
- `Return`

It performs no cross-basic-block elimination, no emitter linear-state inference, no `slotToVReg_` cross-block propagation, no alias analysis, and no register-allocation change.

## Removed Writebacks

Runtime validation used 13 `src` cases. In those cases:

- Direct MIR `StoreFrame` deletions: 0
- VRegHome writebacks suppressed: 79
- Total reported dead writebacks: 79

The direct `StoreFrame` deletion rule is covered by MIR unit tests. The current `-opt` runtime cases primarily expose emitter VRegHome writebacks, so the runtime benefit comes from `suppressVRegHomeStore`.

## Runtime Metrics

| case | before lw | after lw | before sw | after sw | before call | after call | before asm lines | after asm lines | writebacks suppressed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| branch_join_value | 8 | 8 | 21 | 13 | 2 | 2 | 59 | 51 | 8 |
| caller_local_after_call | 5 | 5 | 10 | 6 | 1 | 1 | 32 | 28 | 4 |
| caller_saved_clobber | 18 | 18 | 45 | 26 | 1 | 1 | 109 | 90 | 19 |
| if_else_merge | 1 | 1 | 2 | 1 | 0 | 0 | 10 | 9 | 1 |
| large_local_pressure | 1 | 1 | 2 | 1 | 0 | 0 | 10 | 9 | 1 |
| leaf_s_registers | 1 | 1 | 2 | 1 | 0 | 0 | 10 | 9 | 1 |
| loop_variable_across_call | 9 | 9 | 22 | 15 | 1 | 1 | 57 | 50 | 7 |
| multi_level_calls | 8 | 8 | 16 | 10 | 2 | 2 | 50 | 44 | 6 |
| nonleaf_s_registers | 5 | 5 | 10 | 6 | 1 | 1 | 32 | 28 | 4 |
| recursive_call | 6 | 6 | 16 | 8 | 2 | 2 | 50 | 42 | 8 |
| recursive_fibonacci | 6 | 6 | 17 | 9 | 3 | 3 | 54 | 46 | 8 |
| register_pressure_runtime | 6 | 6 | 15 | 8 | 2 | 2 | 44 | 37 | 7 |
| while_backedge | 6 | 6 | 16 | 11 | 0 | 0 | 39 | 34 | 5 |
| total | 80 | 80 | 194 | 115 | 15 | 15 | 556 | 477 | 79 |

Result:

- Total `sw`: 194 -> 115
- Total `lw`: 80 -> 80
- Total call count: 15 -> 15
- Total assembly lines: 556 -> 477

No case shows `sw` reduction with worse `lw`, call count, or assembly line count.

## Spike Results

Command:

```powershell
powershell -ExecutionPolicy Bypass -File tools\src_backend_regression\run_runtime.ps1 -NoBuild
```

Result:

- `src`: 13/13 PASS
- `src2`: 13/13 PASS
- `src` and `src2`: all shared signed return values are consistent
- `mul/div/rem`: 0/0/0 for every runtime row

The latest machine-readable evidence is in:

- `tools/src_backend_regression/reports/latest_runtime.md`
- `tools/src_backend_regression/reports/runtime/rows.psv`

## src and src2 Difference

`src2` still emits fewer stack accesses on most cases. The gap remains largest on call-heavy and register-pressure cases, especially `caller_saved_clobber`, `loop_variable_across_call`, and recursive cases.

This pass narrows the `src` stack-write gap without changing the ABI baseline. The remaining difference is mainly broader VRegHome materialization and conservative frame traffic in `src`.

## Regression Judgment

No runtime semantic regression appeared in the 13-case Spike validation set.

The optimization is limited to block-local proof:

- Direct `StoreFrame` removal requires same basic block, same frame slot, later overwrite, and no hard boundary.
- VRegHome suppression requires all uses of the VReg to stay inside one basic block segment.
- The emitter skips a VRegHome `sw` only when the VReg has a physical register assignment.

The change is isolated to MIR metadata/pass logic, emitter consumption of that metadata, tests, and runtime reporting, so it can be reverted independently.
