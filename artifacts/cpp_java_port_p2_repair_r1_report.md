# P2-Repair R1 交付报告

范围：多函数标签命名、9+ 参数 FrameLayout、Spike harness、P2-C 语义收尾。

## 1. 审计问题与修复对应表

| 审计问题 | 修复位置 | 修复结果 | 验证 |
|---|---|---|---|
| 多函数重复发射 `entry.0:` | `src/backend/riscv_emitter.cpp`、`include/toyc/backend/riscv_emitter.h` | 所有 block definition 和 branch target 统一走 `blockLabel(function, block/label)`，格式为 `.L<function>__<block>_<ordinal>` | `RiscvEmitterTest.MultiFunctionBlockLabelUniqueness`、`BranchTargetsResolveWithinFunction`、`AllAssemblyLabelsGloballyUnique` |
| 9+ 参数 outgoing area 与 valueHome 重叠 | `src/backend/frame_layout.cpp` | `outgoingArgBytes` 先计算，valueHome 与 allocaHome 从 outgoing area 后开始分配 | 6 个 `FrameLayoutTest.*` |
| Spike 对 0/1/INT_MIN 解析不稳定 | `tools/triple_diff/rv32_split_tohost_start.s`、`rv32_split_tohost_link.ld`、`run_triple_diff.sh`、`rv32_harness_selftest.py` | split64 tohost 协议：高 31 位放 high word，符号 bit 放 low word bit2，low word 为 2/6 | `artifacts/rv32_harness_selftest/selftest_results.psv`，10 次 repeat |
| C++ 接受 `int g;` | `src/frontend/parser.cpp`、`src/frontend/semantic_analyzer.cpp` | parser 保留空 initializer，SemanticAnalyzer 报 `global variable 'g' requires an initializer`，IRBuilder 不进入 | `ParserTest.SemanticRejectsUninitializedGlobal`、`repair_global_uninitialized` |
| void call 作为值表达式后置 IR verifier 报错 | `src/frontend/semantic_analyzer.cpp` | 语义阶段统一报 `void function call cannot be used as a value expression` | 6 个 parser semantic tests、`repair_void_in_value_expr` |

## 2. label namespace 修复前后汇编片段

修复前审计证据：

```asm
add:
entry.0:
  ...

main:
entry.0:
  ...
```

修复后 `tools/triple_diff/reports/runtime/newcpp_cases_repair_multifunction_add.s`：

```asm
add:
.Ladd__entry_0_0:
  ...

main:
.Lmain__entry_0_0:
  ...
```

递归分支目标示例 `newcpp_cases_repair_multifunction_fact.s`：

```asm
fact:
.Lfact__entry_0_0:
  bnez t0, .Lfact__if_then_1_1
  j .Lfact__if_end_2_2
.Lfact__if_then_1_1:
.Lfact__if_end_2_2:
main:
.Lmain__entry_0_0:
```

## 3. FrameLayout 修复前后布局表

修复前：

| 区域 | 起始 offset |
|---|---:|
| valueHome | 0 |
| allocaHome | valueHome 后 |
| outgoing area | valueHome + allocaHome 后 |
| saved s0 | frameSize - 8 |
| saved ra | frameSize - 4 |

修复后：

| 区域 | 起始 offset |
|---|---:|
| outgoing area | 0 |
| valueHome | outgoingArgBytes |
| allocaHome | valueHome 后 |
| saved s0 | frameSize - 8 |
| saved ra | frameSize - 4 |

`repair_sum9` caller `main` 汇编显示 valueHome 从 `4(s0)` 开始，第 9 参数 outgoing slot 使用 `0(s0)`：

```asm
li t0, 1
sw t0, 4(s0)
...
li t0, 9
sw t0, 36(s0)
...
lw t0, 36(s0)
sw t0, 0(s0)
call sum9
sw a0, 40(s0)
```

## 4. region 不重叠证明

新增单元测试枚举区间，而非只比较起点：

```text
FrameLayoutTest.OutgoingAreaStartsAtZero
FrameLayoutTest.ValueHomeAfterOutgoingArea
FrameLayoutTest.AllocaHomeAfterValueHome
FrameLayoutTest.SaveAreaNoOverlap
FrameLayoutTest.AllRegionsNonOverlap
FrameLayoutTest.NestedCallOutgoingArea
```

正式 ctest：`93/93 passed`。

## 5. 9+ 参数 caller 与 callee 汇编示例

Caller `main` (`repair_sum9`)：

```asm
lw a0, 4(s0)
...
lw a7, 32(s0)
lw t0, 36(s0)
sw t0, 0(s0)
call sum9
```

Callee `sum9`：

```asm
sw a0, 68(s0)
...
sw a7, 96(s0)
lw t0, 112(s0)
sw t0, 100(s0)
```

`112(s0)` 等于 callee frameSize + 0，符合第 9 参数读取规则。

## 6. Spike harness 新协议、self-test 结果、解码规则

startup：`tools/triple_diff/rv32_split_tohost_start.s`

编码：

```text
sign = uint32(a0) >> 31
low = 2 | (sign << 2)
high31 = uint32(a0) & 0x7fffffff
tohost64 = (high31 << 32) | low
```

解码：

```text
low == 2 -> sign bit 0
low == 6 -> sign bit 1
uint32 = high31 | (sign << 31)
signed = int32(uint32)
```

self-test：`artifacts/rv32_harness_selftest/selftest_results.psv`

| case | expected | encoded | decoded signed | status |
|---|---:|---|---:|---|
| return_0 | 0 | `0x2` | 0 | PASS |
| return_1 | 1 | `0x100000002` | 1 | PASS |
| return_minus_1 | -1 | `0x7fffffff00000006` | -1 | PASS |
| return_42 | 42 | `0x2a00000002` | 42 | PASS |
| return_int_min | -2147483648 | `0x6` | -2147483648 | PASS |

重复运行：`artifacts/rv32_harness_selftest/repeat_10.log`，10/10 PASS。

## 7. Java/C++ 多函数与 P2-C 双路 Spike 对比表

来源：`tools/triple_diff/reports/runtime/rows.psv`

| case | Java | newcpp | expected | 结论 |
|---|---:|---:|---:|---|
| repair_multifunction_add | 42 | 42 | 42 | PASS |
| repair_multifunction_fact | 120 | 120 | 120 | PASS |
| repair_multifunction_fib | 21 | 21 | 21 | PASS |
| repair_nested_call | 42 | 42 | 42 | PASS |
| repair_void_sink | 42 | 42 | 42 | PASS |
| repair_sum9 | 45 | 45 | 45 | PASS |
| repair_sum12 | 78 | 78 | 78 | PASS |
| repair_forward9 | 45 | 45 | 45 | PASS |
| repair_recursive9 | 24 | 24 | 24 | PASS |
| repair_nested_ids_sum9 | 45 | 45 | 45 | PASS |
| repair_global_write | 42 | 42 | 42 | PASS |
| global_shadow | 10 | 10 | 10 | PASS |
| repair_global_cross_function | 42 | 42 | 42 | PASS |
| repair_global_parameter_shadow | 42 | 42 | 42 | PASS |
| repair_global_ninth_arg | 45 | 45 | 45 | PASS |

## 8. `int g;` 与 void-in-expression 语义对齐证据

Diagnostic case：

| case | Java | newcpp | C++ stderr keyword | 结论 |
|---|---|---|---|---|
| repair_global_uninitialized | compile_error | compile_error | `requires an initializer` | PASS |
| repair_void_in_value_expr | compile_error | compile_error | `void function call cannot be used as a value expression` | PASS |

新增语义单测：

```text
ParserTest.SemanticRejectsUninitializedGlobal
ParserTest.SemanticRejectsVoidCallInReturnValue
ParserTest.SemanticRejectsVoidCallInBinaryLeft
ParserTest.SemanticRejectsVoidCallInBinaryRight
ParserTest.SemanticRejectsVoidCallInInitializer
ParserTest.SemanticRejectsVoidCallAsIntArgument
ParserTest.SemanticAllowsVoidCallExpressionStatement
```

## 9. 测试数量

existing unit tests：77

new R1 unit tests：16
- label namespace：3
- FrameLayout：6
- semantic diagnostics：7

new runtime cases：14

new diagnostic cases：2

total executed checks：
- ctest：93/93 PASS
- harness self-test：5 values x 10 repeats PASS
- R1 newcpp formal cases：16/16 PASS
- R1 Java formal cases：16/16 PASS

全量 triple_diff 当前统计：

```text
java: 68 PASS, 1 FAIL
newcpp: 60 PASS, 9 FAIL
newcpp_native: 60 PASS, 9 FAIL
src2: 67 PASS, 2 SKIP
```

## 10. 已知限制

全量 triple_diff 中剩余 newcpp FAIL 属于 R1 外的既有能力缺口或旧 case：

```text
const_short_circuit_and_rhs_needed.tc   const 未实现
const_short_circuit_or_rhs_needed.tc    const 未实现
short_circuit_div_zero.tc               const 未实现
p1_nested_logic_precedence.tc           旧短路嵌套 lowering/emitter 缺口
p2_both_return_unreachable.tc           if 双 return 后 merge block 终止问题
p2_if_else.tc                           if 双 return 后 merge block 终止问题
p2_if_return.tc                         if 双 return 后 merge block 终止问题
p2_nested_if_while.tc                   嵌套作用域/while emitter 缺口
p1_int_min.tc                           Java 与 C++ 当前均拒绝该字面量
```

这些限制未阻塞 R1 指定的 P2-B/P2-B3/P2-C 收口项。

## 验收命令

已执行：

```powershell
cmake -S . -B build -DTOYC_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure

powershell -ExecutionPolicy Bypass -File tools\triple_diff\run.ps1 `
  -NewCppCompiler build\toycc.exe `
  -CasesDir tools\triple_diff\cases

python tools\triple_diff\rv32_harness_selftest.py
git diff --check
git diff --name-only -- toy-c-compiler-master src2
```

结果：

```text
ctest: 93/93 PASS
R1 formal newcpp cases: 16/16 PASS
harness repeat: 10/10 PASS
git diff --check: PASS
git diff --name-only -- toy-c-compiler-master src2: empty
```

Java source build 在 `tools/triple_diff/reports/runtime/java_build.log` 中仍记录 Maven 编译失败；runner 继续使用既有 `toy-c-compiler-master/target/toyc.jar` 作为 oracle。src2 build 完成，仅有 narrowing warning。

## 结论

P2-B / P2-B3 / P2-C 的 R1 阻塞项已关闭：多函数、递归、void call、9+ 参数、P2-C global read/write、local shadow、parameter shadow、cross-function global、global as ninth argument 均有 Java/C++ 双路 Spike 证据。

进入 P3-A 的条件：具备进入 CFG / DominatorTree / Mem2Reg 前置基础工作的条件；建议在 P3-A 前单独处理全量 triple_diff 中剩余的旧 case 缺口，避免 P3 期间混入 P2 语义债。
