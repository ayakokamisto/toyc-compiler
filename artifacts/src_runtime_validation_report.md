# src runtime validation report

## 1. Environment

| item | value |
| --- | --- |
| Windows repo | `E:\TOYC` |
| WSL repo | `/mnt/e/TOYC` |
| src compiler | `/mnt/e/TOYC/build/toycc.exe` |
| src2 compiler | `/tmp/toyc_src2_runtime/src2_toycc` |
| WSL GCC | `/usr/bin/riscv64-unknown-elf-gcc` |
| GCC version | `riscv64-unknown-elf-gcc (13.2.0-11ubuntu1+12) 13.2.0` |
| Spike | `/home/ayako/.local/bin/spike` |
| Spike version evidence | `Spike RISC-V ISA Simulator 1.1.1-dev` from `spike --help` |
| bare-metal start | `/tmp/rvtest_start.s` |
| linker script | `/tmp/rvtest_link.ld` |
| runtime evidence dir | `tools/src_backend_regression/reports/runtime/` |

`/tmp/rvtest_start.s` is the pre-existing WSL helper. It calls `main`, stores `a0` to `tohost`, then loops. `/tmp/rvtest_link.ld` is the pre-existing linker script with the image base at `0x80000000`.

The runtime command path is:

```powershell
powershell -ExecutionPolicy Bypass -File tools\src_backend_regression\run_runtime.ps1 -NoBuild
```

The WSL runner uses:

```bash
riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib -static -T /tmp/rvtest_link.ld
$HOME/.local/bin/spike --isa=rv32i
```

## 2. Runtime Results

All 12 `src` cases compiled, linked, executed under Spike, produced a `tohost` value, and matched `EXPECT`.

| case | expected | src actual signed | src status | src2 actual signed | src2 status |
| --- | ---: | ---: | --- | ---: | --- |
| branch_join_value | 52 | 52 | PASS | 52 | PASS |
| caller_local_after_call | 19 | 19 | PASS | 19 | PASS |
| caller_saved_clobber | 104 | 104 | PASS | 104 | PASS |
| if_else_merge | 13 | 13 | PASS | 13 | PASS |
| large_local_pressure | 210 | 210 | PASS | 210 | PASS |
| leaf_s_registers | 10 | 10 | PASS | 10 | PASS |
| loop_variable_across_call | 10 | 10 | PASS | 10 | PASS |
| multi_level_calls | 11 | 11 | PASS | 11 | PASS |
| nonleaf_s_registers | 9 | 9 | PASS | 9 | PASS |
| recursive_call | 15 | 15 | PASS | 15 | PASS |
| recursive_fibonacci | 55 | 55 | PASS | 55 | PASS |
| while_backedge | 10 | 10 | PASS | 10 | PASS |

`run_runtime.sh` records both unsigned and signed 32-bit values. Spike sometimes prints odd return values as `*** FAILED *** (tohost = n)`, where the actual return value is `(n << 1) | 1`; the script decodes that form and records the signed/unsigned result.

## 3. Static Metrics

| case | src lw | src sw | src jal | src call | src ret | src mul/div/rem | src2 lw | src2 sw | src2 jal | src2 call | src2 ret | src2 mul/div/rem |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: | --- |
| branch_join_value | 8 | 21 | 0 | 2 | 2 | 0/0/0 | 2 | 2 | 0 | 2 | 2 | 0/0/0 |
| caller_local_after_call | 5 | 10 | 0 | 1 | 2 | 0/0/0 | 2 | 2 | 0 | 1 | 2 | 0/0/0 |
| caller_saved_clobber | 18 | 45 | 0 | 1 | 2 | 0/0/0 | 11 | 11 | 0 | 1 | 2 | 0/0/0 |
| if_else_merge | 1 | 2 | 0 | 0 | 1 | 0/0/0 | 0 | 0 | 0 | 0 | 1 | 0/0/0 |
| large_local_pressure | 1 | 2 | 0 | 0 | 1 | 0/0/0 | 0 | 0 | 0 | 0 | 1 | 0/0/0 |
| leaf_s_registers | 1 | 2 | 0 | 0 | 1 | 0/0/0 | 0 | 0 | 0 | 0 | 1 | 0/0/0 |
| loop_variable_across_call | 9 | 22 | 0 | 1 | 2 | 0/0/0 | 3 | 3 | 0 | 1 | 2 | 0/0/0 |
| multi_level_calls | 8 | 16 | 0 | 2 | 3 | 0/0/0 | 2 | 2 | 0 | 2 | 3 | 0/0/0 |
| nonleaf_s_registers | 5 | 10 | 0 | 1 | 2 | 0/0/0 | 2 | 2 | 0 | 1 | 2 | 0/0/0 |
| recursive_call | 6 | 16 | 0 | 2 | 2 | 0/0/0 | 3 | 3 | 0 | 2 | 2 | 0/0/0 |
| recursive_fibonacci | 6 | 17 | 0 | 3 | 2 | 0/0/0 | 4 | 4 | 0 | 3 | 2 | 0/0/0 |
| while_backedge | 6 | 16 | 0 | 0 | 1 | 0/0/0 | 2 | 2 | 0 | 0 | 1 | 0/0/0 |

## 4. src and src2 Behavior Comparison

All shared cases have matching `src` and `src2` signed return values.

| case | behavior | assembly difference summary |
| --- | --- | --- |
| branch_join_value | consistent | `src` keeps conservative VRegHome traffic; `src2` emits fewer loads/stores. |
| caller_local_after_call | consistent | `src` saves/restores `s*` and writes VRegHome; `src2` uses fewer frame accesses. |
| caller_saved_clobber | consistent | both preserve call-surviving values; `src` has higher spill/store pressure. |
| if_else_merge | consistent | both fold the result; `src` still materializes callee-saved frame traffic. |
| large_local_pressure | consistent | both fold the constant sum; `src` retains minimal frame traffic. |
| leaf_s_registers | consistent | both return 10; `src` proves `s1` save/restore dynamically. |
| loop_variable_across_call | consistent | both preserve induction variable across call; `src` has higher loop stack traffic. |
| multi_level_calls | consistent | both preserve nested call returns; `src` saves/restores more state. |
| nonleaf_s_registers | consistent | both preserve caller local after call; `src` records explicit `ra` and `s*` frame traffic. |
| recursive_call | consistent | both preserve recursive activation records for additive recursion. |
| recursive_fibonacci | consistent | both preserve independent recursive frames and return 55. |
| while_backedge | consistent | both preserve loop backedge state; `src` retains conservative stack synchronization. |

## 5. RV32I Compatibility

All `src` and `src2` runtime assemblies used in this validation linked with `-march=rv32i -mabi=ilp32`. The reported `mul/div/rem` counts are `0/0/0` for all rows, so the runtime set contains no M-extension instruction-set violation.

## 6. ABI Validation Judgment

The dynamic run verifies the `src` ABI repair for the requested risk areas:

- leaf functions using `s1-s11`: `leaf_s_registers`, `large_local_pressure`
- non-leaf `ra` save/restore: `nonleaf_s_registers`, `multi_level_calls`
- caller local after call: `caller_local_after_call`
- caller-saved clobber pressure: `caller_saved_clobber`
- recursive activation records: `recursive_call`, `recursive_fibonacci`
- induction variable across call: `loop_variable_across_call`
- spill/reload pressure: `large_local_pressure`, `caller_saved_clobber`
- branch join value: `branch_join_value`, `if_else_merge`
- loop backedge stack/register consistency: `while_backedge`

ABI repair is now validated by real Spike execution for the covered cases.

## 7. Optimization Gate

The next phase can start the single allowed optimization target: block-local VRegHome dead writeback suppression. The first version should delete only redundant `sw` in the same basic block before overwrite or before block/call/branch materialization boundaries. It should keep cross-basic-block inference and cross-block slot forwarding disabled.

## 8. Failure Notes

No runtime failures remain in the current 12-case validation set. Failure scene preservation is still implemented: each run writes `.tc`, `.s`, `.elf`, `.spike.log`, and `.result.txt` under `tools/src_backend_regression/reports/runtime/`.
