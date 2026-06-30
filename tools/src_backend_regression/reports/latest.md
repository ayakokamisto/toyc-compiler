# src backend regression report

- generated_at: 2026-06-30T16:30:16
- toycc: E:\TOYC\build\toycc.exe
- riscv_gcc: missing
- riscv_qemu: missing

| case | expected | compile | static | runtime | lw | sw | call | mul | div | rem |
| --- | ---: | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| caller_local_after_call | 19 | pass | pass | skipped: no riscv runner | 8 | 18 | 1 | 0 | 0 | 0 |
| if_else_merge | 13 | pass | pass | skipped: no riscv runner | 5 | 17 | 0 | 0 | 0 | 0 |
| leaf_s_registers | 10 | pass | pass | skipped: no riscv runner | 4 | 18 | 0 | 0 | 0 | 0 |
| loop_variable_across_call | 10 | pass | pass | skipped: no riscv runner | 11 | 26 | 1 | 0 | 0 | 0 |
| multi_level_calls | 11 | pass | pass | skipped: no riscv runner | 8 | 20 | 2 | 0 | 0 | 0 |
| nonleaf_s_registers | 9 | pass | pass | skipped: no riscv runner | 8 | 18 | 1 | 0 | 0 | 0 |
| recursive_call | 120 | pass | pass | skipped: no riscv runner | 8 | 20 | 3 | 0 | 0 | 0 |
| while_backedge | 10 | pass | pass | skipped: no riscv runner | 8 | 18 | 0 | 0 | 0 | 0 |

Runtime execution skipped because a local RISC-V compiler and qemu runner were not both available.
