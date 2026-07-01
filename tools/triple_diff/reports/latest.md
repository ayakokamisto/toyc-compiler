# Triple compiler diff report

- generated_at: 2026-07-01T14:21:13+08:00
- repo: /mnt/e/TOYC
- java_root: /mnt/e/TOYC/toy-c-compiler-master
- src2_root: /mnt/e/TOYC/src2
- new_cpp: /mnt/e/TOYC/build/toycc.exe
- cases_root: /mnt/e/TOYC/tools/triple_diff/cases
- gcc: /usr/bin/riscv64-unknown-elf-gcc
- spike: /home/ayako/.local/bin/spike

| case | expected | compiler | status | reason | actual signed | lw | sw | call | mul | div | rem | asm lines |
| --- | ---: | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| const_short_circuit_and_rhs_needed.tc | 1 | java | PASS |  | 1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| const_short_circuit_and_rhs_needed.tc | 1 | src2 | PASS |  | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| const_short_circuit_and_rhs_needed.tc | 1 | newcpp | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| const_short_circuit_and_rhs_needed.tc | 1 | newcpp_native | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| const_short_circuit_or_rhs_needed.tc | 1 | java | PASS |  | 1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| const_short_circuit_or_rhs_needed.tc | 1 | src2 | PASS |  | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| const_short_circuit_or_rhs_needed.tc | 1 | newcpp | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| const_short_circuit_or_rhs_needed.tc | 1 | newcpp_native | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| function_call.tc | 7 | java | PASS |  | 7 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| function_call.tc | 7 | src2 | PASS |  | 7 | 1 | 1 | 1 | 0 | 0 | 0 | 19 |
| function_call.tc | 7 | newcpp | PASS |  | 7 | 12 | 12 | 1 | 0 | 0 | 0 | 40 |
| function_call.tc | 7 | newcpp_native | PASS |  | 7 | 12 | 12 | 1 | 0 | 0 | 0 | 40 |
| global_shadow.tc | 10 | java | PASS |  | 10 | 2 | 2 | 0 | 0 | 0 | 0 | 13 |
| global_shadow.tc | 10 | src2 | PASS |  | 10 | 0 | 0 | 0 | 0 | 0 | 0 | 5 |
| global_shadow.tc | 10 | newcpp | PASS |  | 10 | 8 | 7 | 0 | 0 | 0 | 0 | 24 |
| global_shadow.tc | 10 | newcpp_native | PASS |  | 10 | 8 | 7 | 0 | 0 | 0 | 0 | 24 |
| loop_accumulator.tc | 5050 | java | PASS |  | 5050 | 7 | 7 | 0 | 0 | 0 | 0 | 42 |
| loop_accumulator.tc | 5050 | src2 | PASS |  | 5050 | 2 | 2 | 0 | 0 | 0 | 0 | 23 |
| loop_accumulator.tc | 5050 | newcpp | PASS |  | 5050 | 20 | 19 | 0 | 0 | 0 | 0 | 60 |
| loop_accumulator.tc | 5050 | newcpp_native | PASS |  | 5050 | 20 | 19 | 0 | 0 | 0 | 0 | 60 |
| many_arguments.tc | 45 | java | PASS |  | 45 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| many_arguments.tc | 45 | src2 | PASS |  | 45 | 13 | 13 | 1 | 0 | 0 | 0 | 76 |
| many_arguments.tc | 45 | newcpp | PASS |  | 45 | 41 | 41 | 1 | 0 | 0 | 0 | 112 |
| many_arguments.tc | 45 | newcpp_native | PASS |  | 45 | 41 | 41 | 1 | 0 | 0 | 0 | 112 |
| p1_arithmetic.tc | 10 | java | PASS |  | 12 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_arithmetic.tc | 10 | src2 | PASS |  | 12 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_arithmetic.tc | 10 | newcpp | PASS |  | 12 | 13 | 13 | 0 | 1 | 1 | 1 | 43 |
| p1_arithmetic.tc | 10 | newcpp_native | PASS |  | 12 | 13 | 13 | 0 | 1 | 1 | 1 | 43 |
| p1_comments.tc | 42 | java | PASS |  | 42 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_comments.tc | 42 | src2 | PASS |  | 42 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_comments.tc | 42 | newcpp | PASS |  | 42 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| p1_comments.tc | 42 | newcpp_native | PASS |  | 42 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| p1_comparison.tc | 4 | java | PASS |  | 3 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_comparison.tc | 4 | src2 | PASS |  | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_comparison.tc | 4 | newcpp | PASS |  | 3 | 25 | 25 | 0 | 0 | 0 | 0 | 83 |
| p1_comparison.tc | 4 | newcpp_native | PASS |  | 3 | 25 | 25 | 0 | 0 | 0 | 0 | 83 |
| p1_div_mod.tc | 5 | java | PASS |  | 5 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_div_mod.tc | 5 | src2 | PASS |  | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_div_mod.tc | 5 | newcpp | PASS |  | 5 | 9 | 9 | 0 | 0 | 1 | 1 | 31 |
| p1_div_mod.tc | 5 | newcpp_native | PASS |  | 5 | 9 | 9 | 0 | 0 | 1 | 1 | 31 |
| p1_int_min.tc | -2147483648 | java | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p1_int_min.tc | -2147483648 | src2 | PASS |  | -2147483648 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_int_min.tc | -2147483648 | newcpp | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p1_int_min.tc | -2147483648 | newcpp_native | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p1_logic.tc | 1 | java | PASS |  | 1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_logic.tc | 1 | src2 | PASS |  | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 18 |
| p1_logic.tc | 1 | newcpp | PASS |  | 1 | 13 | 14 | 0 | 0 | 0 | 0 | 50 |
| p1_logic.tc | 1 | newcpp_native | PASS |  | 1 | 13 | 14 | 0 | 0 | 0 | 0 | 50 |
| p1_logic_value_normalization.tc | 1 | java | PASS |  | 1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_logic_value_normalization.tc | 1 | src2 | PASS |  | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 18 |
| p1_logic_value_normalization.tc | 1 | newcpp | PASS |  | 1 | 7 | 8 | 0 | 0 | 0 | 0 | 28 |
| p1_logic_value_normalization.tc | 1 | newcpp_native | PASS |  | 1 | 7 | 8 | 0 | 0 | 0 | 0 | 28 |
| p1_nested_logic_precedence.tc | 1 | java | PASS |  | 1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_nested_logic_precedence.tc | 1 | src2 | PASS |  | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 31 |
| p1_nested_logic_precedence.tc | 1 | newcpp | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p1_nested_logic_precedence.tc | 1 | newcpp_native | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p1_parentheses.tc | 9 | java | PASS |  | 9 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_parentheses.tc | 9 | src2 | PASS |  | 9 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_parentheses.tc | 9 | newcpp | PASS |  | 9 | 7 | 7 | 0 | 1 | 0 | 0 | 25 |
| p1_parentheses.tc | 9 | newcpp_native | PASS |  | 9 | 7 | 7 | 0 | 1 | 0 | 0 | 25 |
| p1_precedence.tc | 7 | java | PASS |  | 7 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_precedence.tc | 7 | src2 | PASS |  | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_precedence.tc | 7 | newcpp | PASS |  | 7 | 7 | 7 | 0 | 1 | 0 | 0 | 25 |
| p1_precedence.tc | 7 | newcpp_native | PASS |  | 7 | 7 | 7 | 0 | 1 | 0 | 0 | 25 |
| p1_return_0.tc | 0 | java | PASS |  | 0 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_return_0.tc | 0 | src2 | PASS |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_return_0.tc | 0 | newcpp | PASS |  | 0 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| p1_return_0.tc | 0 | newcpp_native | PASS |  | 0 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| p1_return_1.tc | 1 | java | PASS |  | 1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_return_1.tc | 1 | src2 | PASS |  | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_return_1.tc | 1 | newcpp | PASS |  | 1 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| p1_return_1.tc | 1 | newcpp_native | PASS |  | 1 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| p1_return_42.tc | 42 | java | PASS |  | 42 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_return_42.tc | 42 | src2 | PASS |  | 42 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_return_42.tc | 42 | newcpp | PASS |  | 42 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| p1_return_42.tc | 42 | newcpp_native | PASS |  | 42 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| p1_return_minus_1.tc | -1 | java | PASS |  | -1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_return_minus_1.tc | -1 | src2 | PASS |  | -1 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_return_minus_1.tc | -1 | newcpp | PASS |  | -1 | 4 | 4 | 0 | 0 | 0 | 0 | 16 |
| p1_return_minus_1.tc | -1 | newcpp_native | PASS |  | -1 | 4 | 4 | 0 | 0 | 0 | 0 | 16 |
| p1_return_minus_42.tc | -42 | java | PASS |  | -42 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_return_minus_42.tc | -42 | src2 | PASS |  | -42 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_return_minus_42.tc | -42 | newcpp | PASS |  | -42 | 4 | 4 | 0 | 0 | 0 | 0 | 16 |
| p1_return_minus_42.tc | -42 | newcpp_native | PASS |  | -42 | 4 | 4 | 0 | 0 | 0 | 0 | 16 |
| p1_short_circuit_and.tc | 0 | java | PASS |  | 0 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_short_circuit_and.tc | 0 | src2 | PASS |  | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 20 |
| p1_short_circuit_and.tc | 0 | newcpp | PASS |  | 0 | 9 | 10 | 0 | 0 | 1 | 0 | 34 |
| p1_short_circuit_and.tc | 0 | newcpp_native | PASS |  | 0 | 9 | 10 | 0 | 0 | 1 | 0 | 34 |
| p1_short_circuit_lhs_false.tc | 0 | java | PASS |  | 0 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_short_circuit_lhs_false.tc | 0 | src2 | PASS |  | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 20 |
| p1_short_circuit_lhs_false.tc | 0 | newcpp | PASS |  | 0 | 9 | 10 | 0 | 0 | 1 | 0 | 34 |
| p1_short_circuit_lhs_false.tc | 0 | newcpp_native | PASS |  | 0 | 9 | 10 | 0 | 0 | 1 | 0 | 34 |
| p1_short_circuit_or.tc | 1 | java | PASS |  | 1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_short_circuit_or.tc | 1 | src2 | PASS |  | 1 | 0 | 0 | 0 | 0 | 1 | 0 | 19 |
| p1_short_circuit_or.tc | 1 | newcpp | PASS |  | 1 | 9 | 10 | 0 | 0 | 1 | 0 | 34 |
| p1_short_circuit_or.tc | 1 | newcpp_native | PASS |  | 1 | 9 | 10 | 0 | 0 | 1 | 0 | 34 |
| p1_short_circuit_rhs_needed.tc | 1 | java | PASS |  | 1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_short_circuit_rhs_needed.tc | 1 | src2 | PASS |  | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 17 |
| p1_short_circuit_rhs_needed.tc | 1 | newcpp | PASS |  | 1 | 9 | 10 | 0 | 0 | 0 | 0 | 34 |
| p1_short_circuit_rhs_needed.tc | 1 | newcpp_native | PASS |  | 1 | 9 | 10 | 0 | 0 | 0 | 0 | 34 |
| p1_unary.tc | 3 | java | PASS |  | 3 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p1_unary.tc | 3 | src2 | PASS |  | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p1_unary.tc | 3 | newcpp | PASS |  | 3 | 6 | 6 | 0 | 0 | 0 | 0 | 22 |
| p1_unary.tc | 3 | newcpp_native | PASS |  | 3 | 6 | 6 | 0 | 0 | 0 | 0 | 22 |
| p2_assignment.tc | 99 | java | PASS |  | 99 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_assignment.tc | 99 | src2 | PASS |  | 99 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p2_assignment.tc | 99 | newcpp | PASS |  | 99 | 6 | 7 | 0 | 0 | 0 | 0 | 21 |
| p2_assignment.tc | 99 | newcpp_native | PASS |  | 99 | 6 | 7 | 0 | 0 | 0 | 0 | 21 |
| p2_both_return_unreachable.tc | 7 | java | PASS |  | 7 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_both_return_unreachable.tc | 7 | src2 | PASS |  | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 13 |
| p2_both_return_unreachable.tc | 7 | newcpp | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p2_both_return_unreachable.tc | 7 | newcpp_native | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p2_dangling_else.tc | 2 | java | PASS |  | 2 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_dangling_else.tc | 2 | src2 | PASS |  | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 22 |
| p2_dangling_else.tc | 2 | newcpp | PASS |  | 2 | 7 | 9 | 0 | 0 | 0 | 0 | 30 |
| p2_dangling_else.tc | 2 | newcpp_native | PASS |  | 2 | 7 | 9 | 0 | 0 | 0 | 0 | 30 |
| p2_if_else.tc | 7 | java | PASS |  | 7 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_if_else.tc | 7 | src2 | PASS |  | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 13 |
| p2_if_else.tc | 7 | newcpp | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p2_if_else.tc | 7 | newcpp_native | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p2_if_return.tc | 7 | java | PASS |  | 7 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_if_return.tc | 7 | src2 | PASS |  | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 13 |
| p2_if_return.tc | 7 | newcpp | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p2_if_return.tc | 7 | newcpp_native | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p2_local_init.tc | 42 | java | PASS |  | 42 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_local_init.tc | 42 | src2 | PASS |  | 42 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p2_local_init.tc | 42 | newcpp | PASS |  | 42 | 5 | 5 | 0 | 0 | 0 | 0 | 17 |
| p2_local_init.tc | 42 | newcpp_native | PASS |  | 42 | 5 | 5 | 0 | 0 | 0 | 0 | 17 |
| p2_many_locals.tc | 55 | java | PASS |  | 55 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_many_locals.tc | 55 | src2 | PASS |  | 55 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p2_many_locals.tc | 55 | newcpp | PASS |  | 55 | 41 | 41 | 0 | 0 | 0 | 0 | 107 |
| p2_many_locals.tc | 55 | newcpp_native | PASS |  | 55 | 41 | 41 | 0 | 0 | 0 | 0 | 107 |
| p2_nested_if_while.tc | 25 | java | PASS |  | 25 | 9 | 9 | 0 | 0 | 0 | 0 | 59 |
| p2_nested_if_while.tc | 25 | src2 | PASS |  | 25 | 3 | 3 | 0 | 0 | 0 | 0 | 37 |
| p2_nested_if_while.tc | 25 | newcpp | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p2_nested_if_while.tc | 25 | newcpp_native | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| p2_nested_scope_initializer.tc | 7 | java | PASS |  | 7 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_nested_scope_initializer.tc | 7 | src2 | PASS |  | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p2_nested_scope_initializer.tc | 7 | newcpp | PASS |  | 7 | 11 | 11 | 0 | 0 | 0 | 0 | 31 |
| p2_nested_scope_initializer.tc | 7 | newcpp_native | PASS |  | 7 | 11 | 11 | 0 | 0 | 0 | 0 | 31 |
| p2_scope_block.tc | 7 | java | PASS |  | 7 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_scope_block.tc | 7 | src2 | PASS |  | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p2_scope_block.tc | 7 | newcpp | PASS |  | 7 | 11 | 11 | 0 | 0 | 0 | 0 | 31 |
| p2_scope_block.tc | 7 | newcpp_native | PASS |  | 7 | 11 | 11 | 0 | 0 | 0 | 0 | 31 |
| p2_scope_shadow.tc | 2 | java | PASS |  | 2 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_scope_shadow.tc | 2 | src2 | PASS |  | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| p2_scope_shadow.tc | 2 | newcpp | PASS |  | 2 | 10 | 11 | 0 | 0 | 0 | 0 | 31 |
| p2_scope_shadow.tc | 2 | newcpp_native | PASS |  | 2 | 10 | 11 | 0 | 0 | 0 | 0 | 31 |
| p2_short_circuit_control.tc | 0 | java | PASS |  | 0 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_short_circuit_control.tc | 0 | src2 | PASS |  | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 28 |
| p2_short_circuit_control.tc | 0 | newcpp | PASS |  | 0 | 10 | 11 | 0 | 0 | 1 | 0 | 39 |
| p2_short_circuit_control.tc | 0 | newcpp_native | PASS |  | 0 | 10 | 11 | 0 | 0 | 1 | 0 | 39 |
| p2_stack_args_forward.tc | 45 | java | PASS |  | 45 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_stack_args_forward.tc | 45 | src2 | PASS |  | 45 | 21 | 21 | 2 | 0 | 0 | 0 | 120 |
| p2_stack_args_forward.tc | 45 | newcpp | PASS |  | 45 | 63 | 63 | 2 | 0 | 0 | 0 | 163 |
| p2_stack_args_forward.tc | 45 | newcpp_native | PASS |  | 45 | 63 | 63 | 2 | 0 | 0 | 0 | 163 |
| p2_stack_args_nested_calls.tc | 45 | java | PASS |  | 45 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_stack_args_nested_calls.tc | 45 | src2 | PASS |  | 45 | 16 | 16 | 10 | 0 | 0 | 0 | 113 |
| p2_stack_args_nested_calls.tc | 45 | newcpp | PASS |  | 45 | 54 | 54 | 10 | 0 | 0 | 0 | 153 |
| p2_stack_args_nested_calls.tc | 45 | newcpp_native | PASS |  | 45 | 54 | 54 | 10 | 0 | 0 | 0 | 153 |
| p2_stack_args_recursion.tc | 24 | java | PASS |  | 24 | 23 | 23 | 1 | 0 | 0 | 0 | 139 |
| p2_stack_args_recursion.tc | 24 | src2 | PASS |  | 24 | 16 | 17 | 1 | 0 | 0 | 0 | 103 |
| p2_stack_args_recursion.tc | 24 | newcpp | PASS |  | 24 | 82 | 72 | 2 | 0 | 0 | 0 | 213 |
| p2_stack_args_recursion.tc | 24 | newcpp_native | PASS |  | 24 | 82 | 72 | 2 | 0 | 0 | 0 | 213 |
| p2_stack_args_sum12.tc | 78 | java | PASS |  | 78 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_stack_args_sum12.tc | 78 | src2 | PASS |  | 78 | 22 | 22 | 1 | 0 | 0 | 0 | 106 |
| p2_stack_args_sum12.tc | 78 | newcpp | PASS |  | 78 | 56 | 56 | 1 | 0 | 0 | 0 | 148 |
| p2_stack_args_sum12.tc | 78 | newcpp_native | PASS |  | 78 | 56 | 56 | 1 | 0 | 0 | 0 | 148 |
| p2_stack_args_sum9.tc | 45 | java | PASS |  | 45 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| p2_stack_args_sum9.tc | 45 | src2 | PASS |  | 45 | 13 | 13 | 1 | 0 | 0 | 0 | 76 |
| p2_stack_args_sum9.tc | 45 | newcpp | PASS |  | 45 | 41 | 41 | 1 | 0 | 0 | 0 | 112 |
| p2_stack_args_sum9.tc | 45 | newcpp_native | PASS |  | 45 | 41 | 41 | 1 | 0 | 0 | 0 | 112 |
| p2_while_break.tc | 5 | java | PASS |  | 5 | 5 | 5 | 0 | 0 | 0 | 0 | 32 |
| p2_while_break.tc | 5 | src2 | PASS |  | 5 | 1 | 1 | 0 | 0 | 0 | 0 | 26 |
| p2_while_break.tc | 5 | newcpp | PASS |  | 5 | 14 | 13 | 0 | 0 | 0 | 0 | 48 |
| p2_while_break.tc | 5 | newcpp_native | PASS |  | 5 | 14 | 13 | 0 | 0 | 0 | 0 | 48 |
| p2_while_continue.tc | 25 | java | PASS |  | 25 | 8 | 8 | 0 | 0 | 0 | 0 | 53 |
| p2_while_continue.tc | 25 | src2 | PASS |  | 25 | 2 | 2 | 0 | 0 | 0 | 1 | 31 |
| p2_while_continue.tc | 25 | newcpp | PASS |  | 25 | 27 | 25 | 0 | 0 | 0 | 1 | 83 |
| p2_while_continue.tc | 25 | newcpp_native | PASS |  | 25 | 27 | 25 | 0 | 0 | 0 | 1 | 83 |
| p2_while_sum.tc | 55 | java | PASS |  | 55 | 7 | 7 | 0 | 0 | 0 | 0 | 42 |
| p2_while_sum.tc | 55 | src2 | PASS |  | 55 | 2 | 2 | 0 | 0 | 0 | 0 | 23 |
| p2_while_sum.tc | 55 | newcpp | PASS |  | 55 | 20 | 19 | 0 | 0 | 0 | 0 | 60 |
| p2_while_sum.tc | 55 | newcpp_native | PASS |  | 55 | 20 | 19 | 0 | 0 | 0 | 0 | 60 |
| recursive_factorial.tc | 120 | java | PASS |  | 120 | 13 | 9 | 2 | 1 | 0 | 0 | 50 |
| recursive_factorial.tc | 120 | src2 | PASS |  | 120 | 3 | 3 | 2 | 1 | 0 | 0 | 35 |
| recursive_factorial.tc | 120 | newcpp | PASS |  | 120 | 22 | 18 | 2 | 1 | 0 | 0 | 70 |
| recursive_factorial.tc | 120 | newcpp_native | PASS |  | 120 | 22 | 18 | 2 | 1 | 0 | 0 | 70 |
| repair_forward9.tc | 45 | java | PASS |  | 45 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| repair_forward9.tc | 45 | src2 | PASS |  | 45 | 21 | 21 | 2 | 0 | 0 | 0 | 120 |
| repair_forward9.tc | 45 | newcpp | PASS |  | 45 | 63 | 63 | 2 | 0 | 0 | 0 | 163 |
| repair_forward9.tc | 45 | newcpp_native | PASS |  | 45 | 63 | 63 | 2 | 0 | 0 | 0 | 163 |
| repair_global_cross_function.tc | 42 | java | PASS |  | 42 | 6 | 5 | 0 | 0 | 0 | 0 | 21 |
| repair_global_cross_function.tc | 42 | src2 | PASS |  | 42 | 2 | 1 | 1 | 0 | 0 | 0 | 19 |
| repair_global_cross_function.tc | 42 | newcpp | PASS |  | 42 | 12 | 11 | 1 | 0 | 0 | 0 | 40 |
| repair_global_cross_function.tc | 42 | newcpp_native | PASS |  | 42 | 12 | 11 | 1 | 0 | 0 | 0 | 40 |
| repair_global_ninth_arg.tc | 45 | java | PASS |  | 45 | 6 | 5 | 0 | 0 | 0 | 0 | 21 |
| repair_global_ninth_arg.tc | 45 | src2 | PASS |  | 45 | 14 | 13 | 1 | 0 | 0 | 0 | 78 |
| repair_global_ninth_arg.tc | 45 | newcpp | PASS |  | 45 | 43 | 42 | 1 | 0 | 0 | 0 | 116 |
| repair_global_ninth_arg.tc | 45 | newcpp_native | PASS |  | 45 | 43 | 42 | 1 | 0 | 0 | 0 | 116 |
| repair_global_parameter_shadow.tc | 42 | java | PASS |  | 42 | 2 | 2 | 0 | 0 | 0 | 0 | 13 |
| repair_global_parameter_shadow.tc | 42 | src2 | PASS |  | 42 | 1 | 1 | 1 | 0 | 0 | 0 | 16 |
| repair_global_parameter_shadow.tc | 42 | newcpp | PASS |  | 42 | 8 | 8 | 1 | 0 | 0 | 0 | 31 |
| repair_global_parameter_shadow.tc | 42 | newcpp_native | PASS |  | 42 | 8 | 8 | 1 | 0 | 0 | 0 | 31 |
| repair_global_uninitialized.tc |  | java | PASS | diagnostic ok |  |  |  |  |  |  |  |  |
| repair_global_uninitialized.tc |  | src2 | SKIP | diagnostic case not run for src2 |  |  |  |  |  |  |  |  |
| repair_global_uninitialized.tc |  | newcpp | PASS | diagnostic ok |  |  |  |  |  |  |  |  |
| repair_global_uninitialized.tc |  | newcpp_native | PASS | diagnostic ok |  |  |  |  |  |  |  |  |
| repair_global_write.tc | 42 | java | PASS |  | 42 | 4 | 4 | 0 | 0 | 0 | 0 | 22 |
| repair_global_write.tc | 42 | src2 | PASS |  | 42 | 2 | 1 | 0 | 0 | 0 | 0 | 13 |
| repair_global_write.tc | 42 | newcpp | PASS |  | 42 | 11 | 10 | 0 | 0 | 0 | 0 | 33 |
| repair_global_write.tc | 42 | newcpp_native | PASS |  | 42 | 11 | 10 | 0 | 0 | 0 | 0 | 33 |
| repair_multifunction_add.tc | 42 | java | PASS |  | 42 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| repair_multifunction_add.tc | 42 | src2 | PASS |  | 42 | 1 | 1 | 1 | 0 | 0 | 0 | 19 |
| repair_multifunction_add.tc | 42 | newcpp | PASS |  | 42 | 12 | 12 | 1 | 0 | 0 | 0 | 40 |
| repair_multifunction_add.tc | 42 | newcpp_native | PASS |  | 42 | 12 | 12 | 1 | 0 | 0 | 0 | 40 |
| repair_multifunction_fact.tc | 120 | java | PASS |  | 120 | 13 | 9 | 2 | 1 | 0 | 0 | 50 |
| repair_multifunction_fact.tc | 120 | src2 | PASS |  | 120 | 3 | 3 | 2 | 1 | 0 | 0 | 35 |
| repair_multifunction_fact.tc | 120 | newcpp | PASS |  | 120 | 22 | 18 | 2 | 1 | 0 | 0 | 70 |
| repair_multifunction_fact.tc | 120 | newcpp_native | PASS |  | 120 | 22 | 18 | 2 | 1 | 0 | 0 | 70 |
| repair_multifunction_fib.tc | 21 | java | PASS |  | 21 | 16 | 11 | 3 | 0 | 0 | 0 | 56 |
| repair_multifunction_fib.tc | 21 | src2 | PASS |  | 21 | 4 | 4 | 3 | 0 | 0 | 0 | 41 |
| repair_multifunction_fib.tc | 21 | newcpp | PASS |  | 21 | 26 | 21 | 3 | 0 | 0 | 0 | 79 |
| repair_multifunction_fib.tc | 21 | newcpp_native | PASS |  | 21 | 26 | 21 | 3 | 0 | 0 | 0 | 79 |
| repair_nested_call.tc | 42 | java | PASS |  | 42 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| repair_nested_call.tc | 42 | src2 | PASS |  | 42 | 2 | 2 | 3 | 0 | 0 | 0 | 31 |
| repair_nested_call.tc | 42 | newcpp | PASS |  | 42 | 18 | 18 | 3 | 0 | 0 | 0 | 60 |
| repair_nested_call.tc | 42 | newcpp_native | PASS |  | 42 | 18 | 18 | 3 | 0 | 0 | 0 | 60 |
| repair_nested_ids_sum9.tc | 45 | java | PASS |  | 45 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| repair_nested_ids_sum9.tc | 45 | src2 | PASS |  | 45 | 16 | 16 | 10 | 0 | 0 | 0 | 113 |
| repair_nested_ids_sum9.tc | 45 | newcpp | PASS |  | 45 | 54 | 54 | 10 | 0 | 0 | 0 | 153 |
| repair_nested_ids_sum9.tc | 45 | newcpp_native | PASS |  | 45 | 54 | 54 | 10 | 0 | 0 | 0 | 153 |
| repair_recursive9.tc | 24 | java | PASS |  | 24 | 23 | 23 | 1 | 0 | 0 | 0 | 139 |
| repair_recursive9.tc | 24 | src2 | PASS |  | 24 | 16 | 17 | 1 | 0 | 0 | 0 | 103 |
| repair_recursive9.tc | 24 | newcpp | PASS |  | 24 | 82 | 72 | 2 | 0 | 0 | 0 | 213 |
| repair_recursive9.tc | 24 | newcpp_native | PASS |  | 24 | 82 | 72 | 2 | 0 | 0 | 0 | 213 |
| repair_sum12.tc | 78 | java | PASS |  | 78 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| repair_sum12.tc | 78 | src2 | PASS |  | 78 | 22 | 22 | 1 | 0 | 0 | 0 | 106 |
| repair_sum12.tc | 78 | newcpp | PASS |  | 78 | 56 | 56 | 1 | 0 | 0 | 0 | 148 |
| repair_sum12.tc | 78 | newcpp_native | PASS |  | 78 | 56 | 56 | 1 | 0 | 0 | 0 | 148 |
| repair_sum9.tc | 45 | java | PASS |  | 45 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| repair_sum9.tc | 45 | src2 | PASS |  | 45 | 13 | 13 | 1 | 0 | 0 | 0 | 76 |
| repair_sum9.tc | 45 | newcpp | PASS |  | 45 | 41 | 41 | 1 | 0 | 0 | 0 | 112 |
| repair_sum9.tc | 45 | newcpp_native | PASS |  | 45 | 41 | 41 | 1 | 0 | 0 | 0 | 112 |
| repair_void_in_value_expr.tc |  | java | PASS | diagnostic ok |  |  |  |  |  |  |  |  |
| repair_void_in_value_expr.tc |  | src2 | SKIP | diagnostic case not run for src2 |  |  |  |  |  |  |  |  |
| repair_void_in_value_expr.tc |  | newcpp | PASS | diagnostic ok |  |  |  |  |  |  |  |  |
| repair_void_in_value_expr.tc |  | newcpp_native | PASS | diagnostic ok |  |  |  |  |  |  |  |  |
| repair_void_sink.tc | 42 | java | PASS |  | 42 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| repair_void_sink.tc | 42 | src2 | PASS |  | 42 | 2 | 3 | 1 | 0 | 0 | 0 | 19 |
| repair_void_sink.tc | 42 | newcpp | PASS |  | 42 | 6 | 7 | 1 | 0 | 0 | 0 | 28 |
| repair_void_sink.tc | 42 | newcpp_native | PASS |  | 42 | 6 | 7 | 1 | 0 | 0 | 0 | 28 |
| return_0.tc | 0 | java | PASS |  | 0 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| return_0.tc | 0 | src2 | PASS |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| return_0.tc | 0 | newcpp | PASS |  | 0 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| return_0.tc | 0 | newcpp_native | PASS |  | 0 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| return_1.tc | 1 | java | PASS |  | 1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| return_1.tc | 1 | src2 | PASS |  | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| return_1.tc | 1 | newcpp | PASS |  | 1 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| return_1.tc | 1 | newcpp_native | PASS |  | 1 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| return_42.tc | 42 | java | PASS |  | 42 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| return_42.tc | 42 | src2 | PASS |  | 42 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| return_42.tc | 42 | newcpp | PASS |  | 42 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| return_42.tc | 42 | newcpp_native | PASS |  | 42 | 3 | 3 | 0 | 0 | 0 | 0 | 13 |
| return_minus_1.tc | -1 | java | PASS |  | -1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| return_minus_1.tc | -1 | src2 | PASS |  | -1 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| return_minus_1.tc | -1 | newcpp | PASS |  | -1 | 4 | 4 | 0 | 0 | 0 | 0 | 16 |
| return_minus_1.tc | -1 | newcpp_native | PASS |  | -1 | 4 | 4 | 0 | 0 | 0 | 0 | 16 |
| return_minus_42.tc | -42 | java | PASS |  | -42 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| return_minus_42.tc | -42 | src2 | PASS |  | -42 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| return_minus_42.tc | -42 | newcpp | PASS |  | -42 | 4 | 4 | 0 | 0 | 0 | 0 | 16 |
| return_minus_42.tc | -42 | newcpp_native | PASS |  | -42 | 4 | 4 | 0 | 0 | 0 | 0 | 16 |
| short_circuit_div_zero.tc | 1 | java | PASS |  | 1 | 2 | 2 | 0 | 0 | 0 | 0 | 12 |
| short_circuit_div_zero.tc | 1 | src2 | PASS |  | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| short_circuit_div_zero.tc | 1 | newcpp | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| short_circuit_div_zero.tc | 1 | newcpp_native | FAIL | compile failure |  | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

## Behavior comparison

| case | java | src2 | newcpp | judgment |
| --- | ---: | ---: | ---: | --- |
| const_short_circuit_and_rhs_needed.tc | 1 | 1 |  | newcpp skipped |
| const_short_circuit_or_rhs_needed.tc | 1 | 1 |  | newcpp skipped |
| function_call.tc | 7 | 7 | 7 | consistent |
| global_shadow.tc | 10 | 10 | 10 | consistent |
| loop_accumulator.tc | 5050 | 5050 | 5050 | consistent |
| many_arguments.tc | 45 | 45 | 45 | consistent |
| p1_arithmetic.tc | 12 | 12 | 12 | consistent |
| p1_comments.tc | 42 | 42 | 42 | consistent |
| p1_comparison.tc | 3 | 3 | 3 | consistent |
| p1_div_mod.tc | 5 | 5 | 5 | consistent |
| p1_int_min.tc |  | -2147483648 |  | insufficient Java/src2 evidence |
| p1_logic.tc | 1 | 1 | 1 | consistent |
| p1_logic_value_normalization.tc | 1 | 1 | 1 | consistent |
| p1_nested_logic_precedence.tc | 1 | 1 |  | newcpp skipped |
| p1_parentheses.tc | 9 | 9 | 9 | consistent |
| p1_precedence.tc | 7 | 7 | 7 | consistent |
| p1_return_0.tc | 0 | 0 | 0 | consistent |
| p1_return_1.tc | 1 | 1 | 1 | consistent |
| p1_return_42.tc | 42 | 42 | 42 | consistent |
| p1_return_minus_1.tc | -1 | -1 | -1 | consistent |
| p1_return_minus_42.tc | -42 | -42 | -42 | consistent |
| p1_short_circuit_and.tc | 0 | 0 | 0 | consistent |
| p1_short_circuit_lhs_false.tc | 0 | 0 | 0 | consistent |
| p1_short_circuit_or.tc | 1 | 1 | 1 | consistent |
| p1_short_circuit_rhs_needed.tc | 1 | 1 | 1 | consistent |
| p1_unary.tc | 3 | 3 | 3 | consistent |
| p2_assignment.tc | 99 | 99 | 99 | consistent |
| p2_both_return_unreachable.tc | 7 | 7 |  | newcpp skipped |
| p2_dangling_else.tc | 2 | 2 | 2 | consistent |
| p2_if_else.tc | 7 | 7 |  | newcpp skipped |
| p2_if_return.tc | 7 | 7 |  | newcpp skipped |
| p2_local_init.tc | 42 | 42 | 42 | consistent |
| p2_many_locals.tc | 55 | 55 | 55 | consistent |
| p2_nested_if_while.tc | 25 | 25 |  | newcpp skipped |
| p2_nested_scope_initializer.tc | 7 | 7 | 7 | consistent |
| p2_scope_block.tc | 7 | 7 | 7 | consistent |
| p2_scope_shadow.tc | 2 | 2 | 2 | consistent |
| p2_short_circuit_control.tc | 0 | 0 | 0 | consistent |
| p2_stack_args_forward.tc | 45 | 45 | 45 | consistent |
| p2_stack_args_nested_calls.tc | 45 | 45 | 45 | consistent |
| p2_stack_args_recursion.tc | 24 | 24 | 24 | consistent |
| p2_stack_args_sum12.tc | 78 | 78 | 78 | consistent |
| p2_stack_args_sum9.tc | 45 | 45 | 45 | consistent |
| p2_while_break.tc | 5 | 5 | 5 | consistent |
| p2_while_continue.tc | 25 | 25 | 25 | consistent |
| p2_while_sum.tc | 55 | 55 | 55 | consistent |
| recursive_factorial.tc | 120 | 120 | 120 | consistent |
| repair_forward9.tc | 45 | 45 | 45 | consistent |
| repair_global_cross_function.tc | 42 | 42 | 42 | consistent |
| repair_global_ninth_arg.tc | 45 | 45 | 45 | consistent |
| repair_global_parameter_shadow.tc | 42 | 42 | 42 | consistent |
| repair_global_uninitialized.tc |  |  |  | insufficient Java/src2 evidence |
| repair_global_write.tc | 42 | 42 | 42 | consistent |
| repair_multifunction_add.tc | 42 | 42 | 42 | consistent |
| repair_multifunction_fact.tc | 120 | 120 | 120 | consistent |
| repair_multifunction_fib.tc | 21 | 21 | 21 | consistent |
| repair_nested_call.tc | 42 | 42 | 42 | consistent |
| repair_nested_ids_sum9.tc | 45 | 45 | 45 | consistent |
| repair_recursive9.tc | 24 | 24 | 24 | consistent |
| repair_sum12.tc | 78 | 78 | 78 | consistent |
| repair_sum9.tc | 45 | 45 | 45 | consistent |
| repair_void_in_value_expr.tc |  |  |  | insufficient Java/src2 evidence |
| repair_void_sink.tc | 42 | 42 | 42 | consistent |
| return_0.tc | 0 | 0 | 0 | consistent |
| return_1.tc | 1 | 1 | 1 | consistent |
| return_42.tc | 42 | 42 | 42 | consistent |
| return_minus_1.tc | -1 | -1 | -1 | consistent |
| return_minus_42.tc | -42 | -42 | -42 | consistent |
| short_circuit_div_zero.tc | 1 | 1 |  | newcpp skipped |
