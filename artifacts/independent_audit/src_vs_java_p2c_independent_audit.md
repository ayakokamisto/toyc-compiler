# src vs Java P2-C 独立审计报告

审计对象：
- Java oracle：`toy-c-compiler-master`
- C++ 新实现：`src`、`include`
- 输出目录：`artifacts/independent_audit`

## 1. 审计范围与不在范围内能力

范围覆盖当前声明完成的 P0/P1/P2-A/P2-B/P2-C 子集：表达式、短路、局部变量、块作用域、赋值、if/while/break/continue、多函数、int/void、参数、递归、9+ 参数、全局变量、全局初始化、GlobalAddr/Load/Store、RV32IM 发射与 Spike 运行。

OUT_OF_SCOPE：数组、指针、SSA 优化、全局数组、性能优化、src2 行为比较、Java oracle 修复、C++ 主线修复。

## 2. 只读约束与实际修改检查

本次写入限定在 `artifacts/independent_audit`。新增和更新内容：
- `audit_cases/*.tc`
- `logs/*.log`、`logs/*.psv`、`logs/*.cmd`
- `java_output/*`
- `cpp_output/*`
- `build_cpp/*`
- `build_java/*`
- `run_audit.ps1`
- `src_vs_java_p2c_independent_audit.md`

只读源码目录未写入。`git status` 显示仓库已有大量脏状态，详见 `logs/git_status_short.log`；这构成 stale artifact 和工作树真实性风险。

## 3. 仓库状态、构建环境、命令与日志位置

环境：
- CMake：`logs/cmake_version.log`
- Maven：`logs/mvn_version.log`
- Java：`logs/java_version.log`
- WSL/RISC-V/Spike：`logs/wsl_toolchain_probe_elevated.log`

C++ 构建：
- Configure：`cmake -S . -B artifacts/independent_audit/build_cpp -G Ninja`
- Build：`cmake --build artifacts/independent_audit/build_cpp`
- 测试：`ctest --test-dir artifacts/independent_audit/build_cpp --output-on-failure`
- 结果：`logs/cpp_configure_rerun.log`、`logs/cpp_build_rerun.log`、`logs/cpp_ctest.log`
- C++ 单元测试：77/77 passed

Java 构建：
- 尝试命令：`mvn package -DskipTests`，并尝试将 `project.build.directory` 和本地 Maven repo 指向 `artifacts/independent_audit/build_java`
- 结果：源码构建失败，缺少 `toyc.semantic`、`toyc.frontend.ast`、ANTLR/parser 等源包；日志 `logs/java_mvn_build_artifact_dir_quoted.log`
- 后续 oracle 运行使用既有 `toy-c-compiler-master/target/toyc.jar`。该 jar 可执行，但存在 stale artifact 风险。

Spike harness：
- 启动代码：WSL `/tmp/rvtest_start.s`
- 链接脚本：WSL `/tmp/rvtest_link.ld`
- 链接命令：`riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static -T /tmp/rvtest_link.ld -o <elf> /tmp/rvtest_start.s <asm>`
- Spike 命令：`timeout 5s /home/ayako/.local/bin/spike --isa=rv32im <elf>`
- 完整矩阵日志：`logs/audit_results.psv`、`logs/run_audit_final_matrix.log`

## 4. Java / C++ 架构映射表

| 模块 | Java 路径与类/方法 | C++ 路径与类/函数 | 对应关系 | 差异 | 风险 |
|---|---|---|---|---|---|
| IR Value identity | `toy-c-compiler-master/src/main/java/toyc/ir/IRBuilder.java` `symbolValues = new IdentityHashMap<>` | `src/ir/*`，IR 按 `Value*` 持有和引用 | 均依赖对象身份表达 IR value | C++ 静态上满足稳定指针模型 | Low |
| Local/Alloca | Java `IRBuilder.newLocal`、`LocalVar` | `src/ir/ir_builder.cpp` `local ... alloca` | 局部变量均形成地址型 value | C++ alloca 放入 entry prefix，IR dump 已确认 | Low |
| Global storage | Java `GlobalVar` + `GlobalAddr` | C++ `module globals` + `GlobalAddrInstr` | 全局作为 module storage，经地址再 load/store | C++ P2-C 动态覆盖不足 | Medium |
| Short-circuit | Java IRBuilder 生成 CFG | C++ `src/ir/ir_builder.cpp` 短路 slot + cbr | 右侧由 CFG 控制 | Spike 对返回 1 的 tohost 解析存在歧义，需汇编/IR 佐证 | Medium |
| Semantic binding | Java semanticResult / Symbol binding | C++ `src/frontend/semantic_analyzer.cpp` 注册 globals/functions/scopes | 名字绑定在语义阶段完成 | C++ `int g;` 与 Java 分类不一致 | Medium |
| Call/Return | Java `Call`/`Return` | C++ `CallInstr`/`ReturnInstr` | IR 层都表达 call operands 和返回值 | C++ 多函数汇编标签重复，动态无法链接 | Critical |
| Frame layout | Java `RiscVEmitter.computeFrameLayout` | C++ `src/backend/frame_layout.cpp` | 均分配 value/local/saved regs/outgoing area | C++ outgoing area 与 value slot 在汇编中重叠 | High |
| a0-a7 参数 | Java `ARG_REGS` | C++ `emit_call_instr` / prologue spill | 前 8 参数进 a0-a7 | 静态一致，动态受标签重复阻塞 | Medium |
| 9+ 参数 | Java caller 临时压栈，callee `frameSize + (i-8)*4` | C++ caller `outgoingArgOffset`，callee `incomingStackArgOffset` | 目标一致 | C++ caller 9th arg 写 `0(s0)`，覆盖 frame value slot | High |
| Global asm | Java `.data` + `asmGlobal` | C++ `.data` + `la` | 均物化全局地址 | 部分 P2-C case Spike timeout / Java compile timeout | Medium |

## 5. 当前支持能力总览

C++ 构建和单元测试通过。动态矩阵确认 P1 普通算术、P2-A 局部/控制流、P2-C 基础全局读写/局部遮蔽在 Java jar 与 C++ 的 RV32IM Spike 行为一致。P2-B 多函数和 9+ 参数当前被汇编重复标签阻塞；9+ 参数汇编还暴露 caller outgoing area 覆盖风险。P2-C 仍处于部分验证状态。

## 6. P0/P1/P2-A/P2-B/P2-C 语义差分表

| 阶段 | 结论 | 证据 |
|---|---|---|
| P0 IR/CFG/Printer/Verifier | LIKELY_EQUIVALENT | C++ 单元 77/77；IR dump 存在并可打印，日志 `cpp_ctest.log` |
| P1 算术/比较/短路 | 部分 CONFIRMED，部分 TOHOST_PARSE_AMBIGUITY | 算术 confirmed；逻辑返回 1 的汇编正确，Spike 空输出导致解析歧义 |
| P2-A 局部/作用域/控制流 | CONFIRMED_EQUIVALENT | `p2_local_init`、`p2_assignment`、`p2_scope_shadow`、`p2_dangling_else`、`p2_while_*` |
| P2-B 多函数/递归/void/参数 | CPP_UNVERIFIED | C++ 汇编器报 `symbol 'entry.0' is already defined` |
| P2-B3 9+ 参数 ABI | CPP_UNVERIFIED / High risk | 同名 label 阻塞链接；汇编显示第 9 参数写 `0(s0)` |
| P2-C 全局变量 | 部分 CONFIRMED，部分 JAVA_UNVERIFIED/CPP_UNVERIFIED | 基础全局读写 confirmed；跨函数/参数遮蔽受多函数标签阻塞；Java `int g;` 编译失败；Java `bump()+bump()` 编译超时 |

## 7. Runtime case 明细

| case | expected | Java | C++ | signed i32 | 结论 |
|---|---:|---|---|---|---|
| p1_return_0 | 0 | compile/link OK, Spike timeout empty | compile/link OK, Spike timeout empty | ambiguous | CPP_UNVERIFIED / TOHOST_PARSE_AMBIGUITY |
| p1_return_42 | 42 | 42 | 42 | 42/42 | CONFIRMED_EQUIVALENT |
| p1_return_minus_1 | -1 | -1 | -1 | -1/-1 | CONFIRMED_EQUIVALENT |
| p1_int_min | -2147483648 | compile error | compile error | n/a | JAVA_UNVERIFIED; both reject literal |
| p1_precedence | 7 | 7 | 7 | 7/7 | CONFIRMED_EQUIVALENT |
| p1_parentheses | 9 | 9 | 9 | 9/9 | CONFIRMED_EQUIVALENT |
| p1_div_mod | 5 | 5 | 5 | 5/5 | CONFIRMED_EQUIVALENT |
| p1_logic_compare | 1 | asm returns 1, Spike empty | asm returns 1, Spike empty | ambiguous | TOHOST_PARSE_AMBIGUITY |
| p1_logic_norm_and | 1 | Spike empty | Spike empty | ambiguous | TOHOST_PARSE_AMBIGUITY |
| p1_logic_norm_or | 1 | Spike empty | Spike empty | ambiguous | TOHOST_PARSE_AMBIGUITY |
| p1_short_and_div0 | 0 | Spike timeout empty | Spike timeout empty | ambiguous | TOHOST_PARSE_AMBIGUITY |
| p1_short_or_div0 | 1 | asm path short-circuits, Spike empty | asm path short-circuits, Spike empty | ambiguous | TOHOST_PARSE_AMBIGUITY |
| p2_local_init | 7 | 7 | 7 | 7/7 | CONFIRMED_EQUIVALENT |
| p2_assignment | 42 | 42 | 42 | 42/42 | CONFIRMED_EQUIVALENT |
| p2_scope_shadow | 2 | 2 | 2 | 2/2 | CONFIRMED_EQUIVALENT |
| p2_dangling_else | 2 | 2 | 2 | 2/2 | CONFIRMED_EQUIVALENT |
| p2_while_continue_sum | 25 | 25 | 25 | 25/25 | CONFIRMED_EQUIVALENT |
| p2_while_break | 3 | 3 | 3 | 3/3 | CONFIRMED_EQUIVALENT |
| p2_add_call | 42 | 42 | link fail | Java 42 | CPP_UNVERIFIED |
| p2_fact | 120 | 120 | link fail | Java 120 | CPP_UNVERIFIED |
| p2_fib | 21 | 21 | link fail | Java 21 | CPP_UNVERIFIED |
| p2_nested_call | 42 | 42 | link fail | Java 42 | CPP_UNVERIFIED |
| p2_void_sink | 42 | 42 | link fail | Java 42 | CPP_UNVERIFIED |
| p2_sum9 | 45 | 45 | link fail | Java 45 | CPP_UNVERIFIED |
| p2_sum12 | 78 | 78 | link fail | Java 78 | CPP_UNVERIFIED |
| p2_sum9_nested_ids | 45 | 45 | link fail | Java 45 | CPP_UNVERIFIED |
| p2_forward9 | 45 | 45 | link fail | Java 45 | CPP_UNVERIFIED |
| p2_sumdown9 | 24 | 24 | link fail | Java 24 | CPP_UNVERIFIED |
| p2c_global_read | 7 | 7 | 7 | 7/7 | CONFIRMED_EQUIVALENT |
| p2c_global_const_expr | 7 | 7 | Spike timeout empty | Java 7 | CPP_UNVERIFIED |
| p2c_global_write | 42 | 42 | 42 | 42/42 | CONFIRMED_EQUIVALENT |
| p2c_global_shadow_local | 42 | 42 | 42 | 42/42 | CONFIRMED_EQUIVALENT |
| p2c_global_shadow_param | 42 | 42 | link fail | Java 42 | CPP_UNVERIFIED |
| p2c_global_state_bump | 5 | compile timeout | link fail | n/a | JAVA_UNVERIFIED |
| p2c_global_cross_fn | 42 | 42 | link fail | Java 42 | CPP_UNVERIFIED |
| p2c_global_ninth_arg | 45 | 45 | link fail | Java 45 | CPP_UNVERIFIED |
| p2c_global_init_short_and | 0 | Spike timeout empty | Spike timeout empty | ambiguous | TOHOST_PARSE_AMBIGUITY |
| p2c_global_init_short_or | 1 | Spike empty | Spike timeout empty | ambiguous | CPP_UNVERIFIED |
| p2c_global_uninit | 0 | compile error | compile/link OK, Spike timeout | n/a | JAVA_UNVERIFIED / semantic mismatch risk |

完整机器可读结果见 `logs/audit_results.psv`。

## 8. Diagnostic case 明细

| case | Java 分类 | C++ 分类 | C++ stderr 关键字 | 结论 |
|---|---|---|---|---|
| diag_redef_local | compile_error | compile_error | redefinition | CONFIRMED_EQUIVALENT |
| diag_undef_var | compile_error | compile_error | undeclared | CONFIRMED_EQUIVALENT |
| diag_break_outside | compile_error | compile_error | break | CONFIRMED_EQUIVALENT |
| diag_continue_outside | compile_error | compile_error | continue | CONFIRMED_EQUIVALENT |
| diag_missing_return | compile_error | compile_error | return | CONFIRMED_EQUIVALENT |
| diag_wrong_arg_count | compile_error | compile_error | expects | CONFIRMED_EQUIVALENT |
| diag_void_return_value | compile_error | compile_error | void | CONFIRMED_EQUIVALENT |
| diag_void_in_expr | compile_error | compile_error | stderr 为 `error: null operand in instruction` | LIKELY_EQUIVALENT，诊断类别不稳定 |
| diag_global_init_id | compile_error | compile_error | constant | CONFIRMED_EQUIVALENT |
| diag_global_init_call | compile_error | compile_error | constant | CONFIRMED_EQUIVALENT |

## 9. IR 对比证据

C++ IR dump：
- 短路：`cpp_output/p1_logic_compare.ir` 显示 `cbr %t4, land.rhs.1, land.end.2`，右侧由 CFG 控制，结果经 slot load 返回。
- 局部遮蔽：`cpp_output/p2_scope_shadow.ir`
- while + continue：`cpp_output/p2_while_continue_sum.ir`
- 递归调用：`cpp_output/p2_fact.ir`
- 9 参数调用：`cpp_output/p2_sum9.ir`
- 全局读取：`cpp_output/p2c_global_read.ir`
- 全局写入：`cpp_output/p2c_global_write.ir` 显示 `global @g = 1`、`globaladdr @g`、`load`、`store`
- 全局与局部同名遮蔽：`cpp_output/p2c_global_shadow_local.ir`

Java 源码静态证据：
- `IRBuilder.java` 使用 `IdentityHashMap<Symbol, IRValue>` 保持 Symbol 与 IR value 身份映射。
- `IRBuilder.java` 对全局用 `GlobalVar`，对全局地址用 `GlobalAddr`。
- Java 当前源树无法从源码重建 jar，动态 oracle 依赖既有 `target/toyc.jar`。

## 10. ABI 与汇编对比证据

叶函数：
- `java_output/p1_return_42.s` 与 `cpp_output/p1_return_42.s` 均可链接并在 Spike 返回 42。

非叶函数：
- Java `p2_add_call` 可链接运行返回 42。
- C++ `p2_add_call.s` 同时输出 `add:` 和 `main:`，两个函数内部都发 `entry.0:`，汇编器报 `symbol 'entry.0' is already defined`。

递归：
- Java `p2_fact` 和 `p2_fib` Spike 返回 120/21。
- C++ 递归 case 均受同名 label 阻塞，结论为 CPP_UNVERIFIED。

9+ 参数：
- C++ `p2_sum9.s` prologue 保存 a0-a7，并从 `112(s0)` 读取第 9 参数。
- C++ caller 在 `main` 中执行 `sw t0, 0(s0)` 后 `call sum9`，该偏移位于 caller value home 区域，属于 frame 覆盖风险。

全局读写：
- `p2c_global_read`、`p2c_global_write` Java/C++ 均链接运行，Spike signed i32 一致。

tohost：
- `/tmp/rvtest_start.s` 将 `a0` 原值写入 `tohost` 后自旋。
- 当前 Spike 对 `tohost=0` 和部分小奇数值存在空输出/timeout 或空输出/exit 0 情况，报告将这类 case 归为 `TOHOST_PARSE_AMBIGUITY`。

## 11. P2-C 全局变量专项审计

已确认：
- `int g = 7; return g;`：Java 7，C++ 7
- `g = g + 41; return g;`：Java 42，C++ 42
- 局部遮蔽全局：Java 42，C++ 42

未验证或风险：
- 全局常量表达式 `1 + 2 * 3`：Java 7；C++ Spike timeout/空输出，需修复 harness 或复核发射。
- 参数遮蔽全局、跨函数全局读取、全局作为第 9 参数：C++ 多函数汇编重复 `entry.0` 阻塞链接。
- `bump()+bump()`：Java jar 编译超时，C++ 受链接失败阻塞。
- `int g;`：Java 编译错误 `no viable alternative at input 'intg;'`，C++ 接受并按 0 初始化，存在语言分类差异风险。

## 12. 已确认一致项

每个确认项均有 `logs/audit_results.psv`、对应 `audit_cases/*.tc`、`java_output/*`、`cpp_output/*` 支撑。

确认一致的 runtime：`p1_return_42`、`p1_return_minus_1`、`p1_precedence`、`p1_parentheses`、`p1_div_mod`、`p2_local_init`、`p2_assignment`、`p2_scope_shadow`、`p2_dangling_else`、`p2_while_continue_sum`、`p2_while_break`、`p2c_global_read`、`p2c_global_write`、`p2c_global_shadow_local`。

确认一致的 diagnostic：除 `diag_void_in_expr` 外的 9 个 diagnostic case。

## 13. 差异项与最小复现

Critical：
- C++ 多函数汇编重复 basic block label。
- 最小复现：`audit_cases/p2_add_call.tc`
- 证据：`cpp_output/p2_add_call.link.log` 报 `symbol 'entry.0' is already defined`。

High：
- C++ 9+ 参数 caller outgoing area 覆盖 caller frame value slot。
- 最小复现：`audit_cases/p2_sum9.tc`
- 证据：`cpp_output/p2_sum9.s` 中第 9 参数写 `0(s0)`；同一函数内 value home 也使用 `0(s0)`。

Medium：
- `int g;` Java 与 C++ 分类存在差异风险。
- 最小复现：`audit_cases/p2c_global_uninit.tc`
- 证据：Java 编译失败，C++ 编译链接成功。

Medium：
- Java jar 在 `p2c_global_state_bump` 编译超时。
- 最小复现：`audit_cases/p2c_global_state_bump.tc`
- 证据：`java_output/p2c_global_state_bump.compile.stderr` 包含 `PROCESS_TIMEOUT after 20s`。

Medium：
- `diag_void_in_expr` C++ 编译失败，但 stderr 为 `error: null operand in instruction`，诊断位置晚于语义分析。
- 最小复现：`audit_cases/diag_void_in_expr.tc`

## 14. 未验证项、阻塞项与原因

TOOLING_BLOCKED：
- Java 源码构建失败，oracle 依赖既有 jar。

HARNESS_BUG_SUSPECTED / TOHOST_PARSE_AMBIGUITY：
- return 0、return 1、短路到 0/1 的 Spike 输出存在空输出、timeout 或 exit 0 歧义。

CPP_UNVERIFIED：
- 所有多函数、递归、void call、9+ 参数、跨函数全局 case 被重复 `entry.0` label 阻塞。

JAVA_UNVERIFIED：
- `p1_int_min`、`p2c_global_state_bump`、`p2c_global_uninit`。

## 15. 风险分级

Critical：1
- C++ 多函数汇编标签重复，P2-B 动态闭环失败。

High：1
- C++ 9+ 参数 outgoing area 与 caller frame value slot 重叠。

Medium：5
- Java 源码构建不可复现。
- tohost harness 对 0/1 解析不稳定。
- P2-C `int g;` 分类差异风险。
- P2-C `bump()+bump()` Java 编译超时。
- `diag_void_in_expr` C++ 诊断类别不稳定。

Low：2
- 仓库脏状态/stale artifact 风险。
- C++ `.global` 与 Java `.globl` 风格差异需统一审查，但当前非功能阻塞。

## 16. 对后续工作的建议顺序

1. 修复审计 harness 的 tohost 协议，给 return 0/1 明确可解析路径。
2. 修复 C++ emitter basic block label 命名，保证跨函数全局唯一或局部 label 化。
3. 复核 C++ FrameLayout outgoing area 与 valueHome/allocaHome/save area 的不重叠约束，重点验证 9+ 参数。
4. 重新跑 P2-B/P2-B3 全矩阵。
5. 明确 P2-C `int g;` 语法规则，并让 Java/C++ 分类一致。
6. 补 P2-C runtime/diagnostic case 到正式测试集。

总体结论：
- 已确认等价：P1 基础算术、P2-A 局部/控制流、P2-C 基础全局读写/局部遮蔽，以及 9 个 diagnostic case。
- 可能等价但证据不足：短路逻辑 IR/汇编显示语义正确，但 Spike tohost 对 0/1 的输出不可稳定解析。
- 已发现语义差异：C++ 接受 `int g;` 且 Java jar 拒绝；`diag_void_in_expr` C++ 诊断类别不稳定。
- 当前 C++ 已实现但未验证：P2-B 多函数/递归/void call、P2-B3 9+ 参数、P2-C 跨函数全局和全局作为第 9 参数。
- 当前 Java 行为仍不确定：源码构建不可复现；`p2c_global_state_bump` 编译超时；`p1_int_min` 拒绝原因来自整数解析异常。
- 是否建议继续 P2-C 收尾：建议继续，但先处理 emitter label、9+ 参数 frame、tohost harness。
- 是否建议进入 P3：暂缓进入 P3。
