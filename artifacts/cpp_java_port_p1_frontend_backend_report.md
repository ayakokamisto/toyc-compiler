# P1-A 完成报告: Toy-C 前端桥接与 RV32IM 端到端闭环

## 支持子集

### 词法
空白、`//` 单行注释、`/* */` 块注释。
Token: `Eof, Identifier, IntegerLiteral, KwInt, KwReturn, LParen, RParen, LBrace, RBrace, Semicolon, Plus, Minus, Star, Slash, Percent, Bang, Less, Greater, LessEqual, GreaterEqual, EqualEqual, BangEqual, AmpAmp, PipePipe`

### 语法
唯一的合法顶层结构：`int main() { return <integer expression>; }`

### 表达式（完整优先级链）
| 优先级 | 运算符 | 结合性 |
|--------|--------|--------|
| 1 (最低) | `\|\|` | 左结合 |
| 2 | `&&` | 左结合 |
| 3 | `==`, `!=` | 左结合 |
| 4 | `<`, `<=`, `>`, `>=` | 左结合 |
| 5 | `+`, `-` | 左结合 |
| 6 | `*`, `/`, `%` | 左结合 |
| 7 (一元) | `+`, `-`, `!` | 右结合 |
| 8 (Primary) | 整数, `(expr)` | — |

### 语法检查
- 函数名不是 `main` → 诊断
- 多余 token、额外函数 → 诊断
- 缺失分号、括号 → 诊断
- 超出 int32 范围的字面量 → 诊断

### INT_MIN 处理
```
-2147483648 → IntLiteralExpr(INT32_MIN)
+2147483648 → range diagnostic
2147483648 (bare) → range diagnostic
-2147483649 → range diagnostic
```

### 暂不支持（预期 P1-B 或 P2）
- `&&` / `||` 短路逻辑（P1-B）
- 变量声明、赋值
- 函数调用、参数
- if/else、while、break、continue
- 全局变量、数组、const

## 模块清单

### 新增文件

```
include/toyc/frontend/token.h        — TokenKind 枚举 + Token 结构
include/toyc/frontend/ast.h          — AST 节点（unique_ptr 树）
include/toyc/frontend/lexer.h        — Lexer 类
include/toyc/frontend/parser.h       — 递归下降 Parser
include/toyc/ir/ir_builder.h         — AST → C++ IR
include/toyc/backend/frame_layout.h  — 预计算 stack frame
include/toyc/backend/riscv_emitter.h — RV32IM 汇编发射器

src/frontend/lexer.cpp               — Lexer 实现
src/frontend/parser.cpp              — Parser 实现
src/ir/ir_builder.cpp                — IRBuilder 实现
src/backend/frame_layout.cpp         — FrameLayout 实现
src/backend/riscv_emitter.cpp        — RV32IM emitter 实现
src/driver/main.cpp                  — 重写（支持 --dump-* / 编译）

tests/frontend/lexer_test.cpp        — 10 个 Lexer 测试
tests/frontend/parser_test.cpp       — 15 个 Parser 测试
tests/ir/ir_builder_test.cpp         — 9 个 IRBuilder 测试
tests/backend/riscv_emitter_test.cpp — 10 个 Emitter 测试

tools/triple_diff/cases/p1_*.tc + .expected.exit — 13 个 runtime case
```

### 修改文件
```
CMakeLists.txt                       — 新增源文件和 4 个测试目标
tools/triple_diff/run_triple_diff.sh — 新增 newcpp_native lane
```

## AST → IR 规则

| AST | IR 指令 |
|-----|---------|
| `IntLiteral(n)` | `LoadImm %t, n` |
| `UnaryExpr(Plus, e)` | 透传 emitExpr(e) |
| `UnaryExpr(Minus, e)` | `UnaryOp(Neg, %t, e)` |
| `UnaryExpr(Not, e)` | `UnaryOp(Not, %t, e)` |
| `BinaryExpr(Add/Sub/Mul/Div/Mod)` | `BinaryOpInstr` |
| `BinaryExpr(Lt/Gt/Le/Ge/Eq/Ne)` | `CompareInstr` |
| `BinaryExpr(And/Or)` | P1-A 诊断拒绝，P1-B 启用 |

每个 `Value*` 恰好一个 defining instruction。

## Frame Layout 规则

```
valueHome:    [Value* → sp-relative offset]（顺序分配 4 字节每 slot）
allocaHome:   [AllocaInstr* → sp-relative offset]（紧随 value homes）
frameSize:    alignUp(totalBytes, 16)
```

约束：
- Alloca 地址值仅 `allocaHome`，不进入 `valueHome`
- `frameSize <= 2047`（signed 12-bit immediate 限制）
- Load/Store address 必须命中 `allocaHome`

## RV32IM 指令选择

| IR | RV32IM | 说明 |
|----|--------|------|
| `LoadImm %t, C` | `li t0, C` → `sw t0, home(%t)(sp)` | 写入 home slot |
| `BinaryOp(Add)` | `lw t0, left(sp)` `lw t1, right(sp)` → `add t2, t0, t1` → `sw t2, result(sp)` | 全精度 32-bit |
| `BinaryOp(Sub)` | 同上，`sub` | — |
| `BinaryOp(Mul)` | 同上，`mul` | RV32IM M 扩展 |
| `BinaryOp(Div)` | 同上，`div` | RV32IM M 扩展 |
| `BinaryOp(Mod)` | 同上，`rem` | RV32IM M 扩展 |
| `UnaryOp(Neg)` | `lw t0, src(sp)` → `neg t2, t0` → `sw t2, result(sp)` | — |
| `UnaryOp(Not)` | `lw t0, src(sp)` → `seqz t2, t0` → `sw t2, result(sp)` | 0/1 归一化 |
| `Compare(Lt)` | `lw ...` → `slt t2, t0, t1` → `sw ...` | 0/1 输出 |
| `Compare(Gt)` | `lw ...` → `slt t2, t1, t0` → `sw ...` | 反向操作数 |
| `Compare(Le)` | `lw ...` → `slt t2, t1, t0` → `xori t2, t2, 1` → `sw ...` | not(b < a) |
| `Compare(Ge)` | `lw ...` → `slt t2, t0, t1` → `xori t2, t2, 1` → `sw ...` | not(a < b) |
| `Compare(Eq)` | `lw ...` → `xor t2, t0, t1` → `seqz t2, t2` → `sw ...` | — |
| `Compare(Ne)` | `lw ...` → `xor t2, t0, t1` → `snez t2, t2` → `sw ...` | — |
| `Branch` | `j label` | — |
| `CondBranch` | `lw t0, cond(sp)` → `bnez t0, true` → `j false` | — |
| `Return %v` | `lw a0, home(%v)(sp)` → epilogue → `ret` | — |
| `Alloca` | 无指令（仅预留 home slot） | — |
| `Store` / `Load` | `lw` / `sw` via allocaHome | 仅 local alloca |

## 与 Java oracle 设计差异

| 维度 | Java | 新 C++ (P1-A) |
|------|------|----------------|
| 寄存器分配 | Union-find + scoring, ~20 regs | 固定 frame-home (t0/t1/t2 scratch) |
| 帧指针 | s0 帧指针 | sp-relative，无 s0 |
| ra 保存 | 有 | 无（P1 无函数调用） |
| 常量特化 | addi/slli/xori 优化 | 无优化 |
| 短路逻辑 | 共享 Temp | 显式 Alloca + Store + Load（P1-B） |

## 测试结果

```
72/72 tests passed (100%)

- P0 IR model: 15 tests
- P0 CFG: 7 tests
- P1 Lexer: 10 tests
- P1 Parser: 15 tests
- P1 IRBuilder: 13 tests (P1-A: 9 + P1-B: 4)
- P1 RiscvEmitter: 12 tests (P1-A: 10 + P1-B: 2)
```

### 关键测试覆盖
- 值身份：同名不同址
- Use-def：全部 13 种指令
- IR printer 稳定性
- 所有权生命周期
- CFG 分支/循环/不可达
- Lexer: 整数/关键字/注释/非法字符
- Parser: 优先级/括号/错误诊断/INT_MIN
- IRBuilder: 算术/比较/一元/IR 拒绝逻辑运算符
- Emitter: li/add/slt/neg/seqz/ret/a0/frame

### `--dump-ir-smoke` 输出
```
func @main() -> i32 {
  local %x.0
entry.0:
  %x.0 = alloca
  %t0 = imm 3
  store %t0, %x.0
  %t1 = load %x.0
  %t3 = imm 5
  %t2 = cmp lt %t1, %t3
  cbr %t2, if.then.1, if.else.2
if.then.1:
  %t4 = load %x.0
  %t6 = imm 1
  %t5 = add %t4, %t6
  ret %t5
if.else.2:
  %t7 = imm 0
  ret %t7
}
```

### 示例汇编输出
```asm
.text
.global main
main:
entry.0:
addi sp, sp, -32
  li t0, 1
  sw t0, 0(sp)
  li t0, 2
  sw t0, 4(sp)
  li t0, 3
  sw t0, 8(sp)
  lw t0, 4(sp)
  lw t1, 8(sp)
  mul t2, t0, t1
  sw t2, 12(sp)
  lw t0, 0(sp)
  lw t1, 12(sp)
  add t2, t0, t1
  sw t2, 16(sp)
  lw a0, 16(sp)
  addi sp, sp, 32
  ret
```

## P1 测试用例

### P1-A（基础算术/比较）

| 文件 | 期望 | 说明 |
|------|------|------|
| `p1_return_0.tc` | 0 | 基础，tohost=0 协议 |
| `p1_return_1.tc` | 1 | 最小正数 |
| `p1_return_42.tc` | 42 | 小正整数 |
| `p1_return_minus_1.tc` | -1 | 负数（Spike 有符号） |
| `p1_return_minus_42.tc` | -42 | 大负数 |
| `p1_precedence.tc` | 7 | 1+2*3 |
| `p1_parentheses.tc` | 9 | (1+2)*3 |
| `p1_unary.tc` | 3 | -5+8 |
| `p1_arithmetic.tc` | 10 | 2*3+8/4+10%6 |
| `p1_comparison.tc` | 4 | (3<5)+(3>5)+... |
| `p1_div_mod.tc` | 5 | 17/5+17%5 |
| `p1_int_min.tc` | -2147483648 | INT_MIN 边界 |
| `p1_comments.tc` | 42 | 注释跳过 |

### P1-B（短路逻辑）

| 文件 | 期望 | 说明 |
|------|------|------|
| `p1_logic.tc` | 1 | `3<4 && 5!=6` |
| `p1_short_circuit_and.tc` | 0 | `0 && (1/0)` — 除零不可达 |
| `p1_short_circuit_or.tc` | 1 | `1 \|\| (1/0)` — 除零不可达 |
| `p1_nested_logic_precedence.tc` | 1 | `1 \|\| 0 && 0` |
| `p1_logic_value_normalization.tc` | 1 | `7 && 9` — 结果归一化为 0/1 |
| `p1_short_circuit_rhs_needed.tc` | 1 | `0 \|\| (8-3)` |
| `p1_short_circuit_lhs_false.tc` | 0 | `0 && (1/0)` — 除零不可达 |

## P1 已实现

- [x] 词法分析（Lexer）：标识符、关键字、整数字面量、全部运算符、注释
- [x] 语法分析（Parser）：递归下降，完整优先级链
- [x] INT_MIN 边界处理（`-2147483648`）
- [x] AST Printer（`--dump-ast`）
- [x] IR Builder：算术、比较、一元运算
- [x] IR Builder：`&&` / `||` 短路 CFG（entry alloca + store/load 结果槽）
- [x] FrameLayout（顺序分配，valueHome + allocaHome 分离）
- [x] 固定 frame-home RV32IM Emitter（sp-relative，t0/t1/t2 scratch）
- [x] IR Verifier（verifyIR + verifyP1EmitterSupport）
- [x] Driver（--dump-tokens, --dump-ast, --dump-ir, --dump-ir-smoke, 编译）
- [x] 20 个端到端测试用例（13 P1-A + 7 P1-B）
- [x] Triple diff runner（newcpp_native lane）
- [x] 所有短路 case 除法块保留在 IR 但运行时不可达

## 已知限制

- 无变量声明 / 赋值 — P2
- 无函数调用 / 参数 — P2
- 无 if-else / while / break / continue — P2
- 无全局变量 / const — P2
- 无 Phi 支持 — P2 Mem2Reg
- Frame size 限制 ≤ 2047 字节
- 无寄存器分配优化（frame-home 保守模型）

## 下一阶段目标

扩展前端与 IRBuilder：
局部变量、块作用域、赋值、函数参数、函数调用
→ 引入 ra 保存、参数寄存器、栈上传参和最小调用约定
