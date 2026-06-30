# src backend Spike runtime report

- generated_at: 2026-06-30T21:55:01+08:00
- repo: /mnt/e/TOYC
- src_toycc: /mnt/e/TOYC/build/toycc.exe
- src2_toycc: /tmp/toyc_src2_runtime/src2_toycc
- gcc_path: /usr/bin/riscv64-unknown-elf-gcc
- gcc_version: riscv64-unknown-elf-gcc (13.2.0-11ubuntu1+12) 13.2.0
- spike_path: /home/ayako/.local/bin/spike
- spike_help: Spike RISC-V ISA Simulator 1.1.1-dev
- start_file: /tmp/rvtest_start.s
- link_file: /tmp/rvtest_link.ld
- runtime_dir: /mnt/e/TOYC/tools/src_backend_regression/reports/runtime

## src2 build
- status: pass

| compiler | case | expected | status | classification | actual unsigned 32-bit | actual signed 32-bit | lw | sw | jal | call | ret | mul | div | rem | asm lines | dead writebacks removed |
| --- | --- | ---: | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| src | branch_join_value | 52 | PASS |  | 52 | 52 | 8 | 21 | 0 | 2 | 2 | 0 | 0 | 0 | 59 | 0 |
| src2 | branch_join_value | 52 | PASS |  | 52 | 52 | 2 | 2 | 0 | 2 | 2 | 0 | 0 | 0 | 37 | 0 |
| src | caller_local_after_call | 19 | PASS |  | 19 | 19 | 5 | 10 | 0 | 1 | 2 | 0 | 0 | 0 | 32 | 0 |
| src2 | caller_local_after_call | 19 | PASS |  | 19 | 19 | 2 | 2 | 0 | 1 | 2 | 0 | 0 | 0 | 20 | 0 |
| src | caller_saved_clobber | 104 | PASS |  | 104 | 104 | 18 | 45 | 0 | 1 | 2 | 0 | 0 | 0 | 109 | 0 |
| src2 | caller_saved_clobber | 104 | PASS |  | 104 | 104 | 11 | 11 | 0 | 1 | 2 | 0 | 0 | 0 | 71 | 0 |
| src | if_else_merge | 13 | PASS |  | 13 | 13 | 1 | 2 | 0 | 0 | 1 | 0 | 0 | 0 | 10 | 0 |
| src2 | if_else_merge | 13 | PASS |  | 13 | 13 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 18 | 0 |
| src | large_local_pressure | 210 | PASS |  | 210 | 210 | 1 | 2 | 0 | 0 | 1 | 0 | 0 | 0 | 10 | 0 |
| src2 | large_local_pressure | 210 | PASS |  | 210 | 210 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 4 | 0 |
| src | leaf_s_registers | 10 | PASS |  | 10 | 10 | 1 | 2 | 0 | 0 | 1 | 0 | 0 | 0 | 10 | 0 |
| src2 | leaf_s_registers | 10 | PASS |  | 10 | 10 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 4 | 0 |
| src | loop_variable_across_call | 10 | PASS |  | 10 | 10 | 9 | 22 | 0 | 1 | 2 | 0 | 0 | 0 | 57 | 0 |
| src2 | loop_variable_across_call | 10 | PASS |  | 10 | 10 | 3 | 3 | 0 | 1 | 2 | 0 | 0 | 0 | 34 | 0 |
| src | multi_level_calls | 11 | PASS |  | 11 | 11 | 8 | 16 | 0 | 2 | 3 | 0 | 0 | 0 | 50 | 0 |
| src2 | multi_level_calls | 11 | PASS |  | 11 | 11 | 2 | 2 | 0 | 2 | 3 | 0 | 0 | 0 | 32 | 0 |
| src | nonleaf_s_registers | 9 | PASS |  | 9 | 9 | 5 | 10 | 0 | 1 | 2 | 0 | 0 | 0 | 32 | 0 |
| src2 | nonleaf_s_registers | 9 | PASS |  | 9 | 9 | 2 | 2 | 0 | 1 | 2 | 0 | 0 | 0 | 20 | 0 |
| src | recursive_call | 15 | PASS |  | 15 | 15 | 6 | 16 | 0 | 2 | 2 | 0 | 0 | 0 | 50 | 0 |
| src2 | recursive_call | 15 | PASS |  | 15 | 15 | 3 | 3 | 0 | 2 | 2 | 0 | 0 | 0 | 35 | 0 |
| src | recursive_fibonacci | 55 | PASS |  | 55 | 55 | 6 | 17 | 0 | 3 | 2 | 0 | 0 | 0 | 54 | 0 |
| src2 | recursive_fibonacci | 55 | PASS |  | 55 | 55 | 4 | 4 | 0 | 3 | 2 | 0 | 0 | 0 | 41 | 0 |
| src | register_pressure_runtime | 142 | PASS |  | 142 | 142 | 6 | 15 | 0 | 2 | 2 | 0 | 0 | 0 | 44 | 0 |
| src2 | register_pressure_runtime | 142 | PASS |  | 142 | 142 | 2 | 2 | 0 | 2 | 2 | 0 | 0 | 0 | 28 | 0 |
| src | while_backedge | 10 | PASS |  | 10 | 10 | 6 | 16 | 0 | 0 | 1 | 0 | 0 | 0 | 39 | 0 |
| src2 | while_backedge | 10 | PASS |  | 10 | 10 | 2 | 2 | 0 | 0 | 1 | 0 | 0 | 0 | 23 | 0 |

## src/src2 behavior comparison

| case | src status | src actual signed | src2 status | src2 actual signed | comparison |
| --- | --- | ---: | --- | ---: | --- |
| branch_join_value | PASS  | 52 | PASS  | 52 | consistent |
| caller_local_after_call | PASS  | 19 | PASS  | 19 | consistent |
| caller_saved_clobber | PASS  | 104 | PASS  | 104 | consistent |
| if_else_merge | PASS  | 13 | PASS  | 13 | consistent |
| large_local_pressure | PASS  | 210 | PASS  | 210 | consistent |
| leaf_s_registers | PASS  | 10 | PASS  | 10 | consistent |
| loop_variable_across_call | PASS  | 10 | PASS  | 10 | consistent |
| multi_level_calls | PASS  | 11 | PASS  | 11 | consistent |
| nonleaf_s_registers | PASS  | 9 | PASS  | 9 | consistent |
| recursive_call | PASS  | 15 | PASS  | 15 | consistent |
| recursive_fibonacci | PASS  | 55 | PASS  | 55 | consistent |
| register_pressure_runtime | PASS  | 142 | PASS  | 142 | consistent |
| while_backedge | PASS  | 10 | PASS  | 10 | consistent |

## Failure evidence

