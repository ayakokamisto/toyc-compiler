# C++ IR Port — P0 完成报告

## Java IR → C++ IR 映射

详情见 [artifacts/java_ir_to_cpp_design.md](java_ir_to_cpp_design.md)。

### 类型映射汇总

| Java | C++ | 差异 |
|------|-----|------|
| `Type.INT` / `Type.VOID` | `Type::Int` / `Type::Void` | `to_string()` 显示 "i32"/"void" |
| `IRValue` (abstract) | `Value` (abstract) | 禁用拷贝，指针身份 |
| `Temp` | `Temp` | 通过 `Function::new_temp()` 在 arena 分配 |
| `LocalVar` | `LocalVar` | 同上 |
| `GlobalVar` | `GlobalVar` | 通过 `Module::new_global_var()` 分配 |
| `Constant` | `Constant` | Factory `Constant::of()` / `Constant::named()` |
| `Label` | `Label` | 通过 `Function::new_label()` 分配 |
| `Instruction` | `Instr` (abstract) | 纯虚接口 |
| `Alloca` | `AllocaInstr` | 命名加 `Instr` 后缀防冲突（`alloca` 是 POSIX 函数名） |
| `Load` | `LoadInstr` | — |
| `Store` | `StoreInstr` | — |
| `LoadImm` | `LoadImmInstr` | — |
| `BinaryOp` | `BinaryOpInstr` | — |
| `UnaryOp` | `UnaryOpInstr` | — |
| `Compare` | `CompareInstr` | — |
| `Call` | `CallInstr` | — |
| `Move` | `MoveInstr` | — |
| `Phi` | `PhiInstr` | `add_incoming(Label*, Value*)` |
| `Branch` | `BranchInstr` | — |
| `CondBranch` | `CondBranchInstr` | — |
| `Return` | `ReturnInstr` | — |
| `GlobalAddr` | `GlobalAddrInstr` | — |
| `BasicBlock` | `BasicBlock` | phi/list/terminator 分离，`all_instrs()` |
| `Function` | `Function` | 内部 value arena，`new_*` factory |
| `Module` | `Module` | 全局变量/常量加 Function 容器 |
| `IRProgram` | `IRProgram` | Module wrapper |
| `IRVisitor` | (无对应) | Printer 使用 switch-on-kind 而非 visitor |
| `ControlFlowGraph` | `CFG` | `CFG::build(Function&)` |

### 值身份规则

Java 使用对象引用身份，C++ 使用 `Value*` 指针身份。`Temp` 和 `LocalVar` 的显示名与身份分离——两个同名值若在不同地址则是不同值。

### 所有权模型

```
Module
  ├── vector<unique_ptr<Function>> functions_
  ├── vector<unique_ptr<Value>> owned_values_  (GlobalVar, module-level Constant)
  └── raw ptr vectors for globals_, global_constants_

Function
  ├── vector<unique_ptr<BasicBlock>> blocks_
  ├── vector<unique_ptr<Value>> owned_values_  (Temp, LocalVar, Label, Constant)
  ├── unordered_map<string, LocalVar*> local_map_
  └── Temp/LocalVar/Label/Constant factory methods (arena allocation)

BasicBlock
  ├── vector<unique_ptr<Instr>> phis_
  ├── vector<unique_ptr<Instr>> instrs_
  └── unique_ptr<Instr> terminator_
```

所有跨层引用使用 raw 指针，生命周期由 `unique_ptr` 链保证。

---

## 文件清单

```
include/toyc/ir/
├── type.h               — Type 枚举 (Int/Void)
├── value.h              — Value, Temp, LocalVar, GlobalVar, Constant, Label
├── instruction.h        — Instr 基类 + 14 种指令子类
├── basic_block.h        — BasicBlock (phi/list/terminator)
├── function.h           — Function (value arena, factory methods)
├── module.h             — Module, IRProgram
├── cfg.h                — CFG 构建器
└── ir_printer.h         — IR Printer (static methods)

src/ir/
├── value.cpp            — Value constructor
├── instruction.cpp      — Instr 子类方法实现
├── basic_block.cpp      — BasicBlock 方法
├── function.cpp         — Function + arena factory
├── module.cpp           — Module + global arena
├── cfg.cpp              — CFG::build() + successor_labels()
└── ir_printer.cpp       — printer (switch-on-kind)

src/driver/
└── main.cpp             — --dump-ir-smoke 入口

tests/ir/
├── ir_model_test.cpp    — 15 tests (value identity, use-def, printer, ownership)
└── cfg_test.cpp         — 7 tests (branch, loop, unreachable, smoke)

CMakeLists.txt           — 构建配置 (C++20, GoogleTest)
```

---

## 构建命令

```bash
cd /mnt/e/TOYC
cmake -S . -B build -DTOYC_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/toycc.exe --dump-ir-smoke
```

---

## 测试结果

### IR Model Tests (15/15 通过)

| Test Suite | 测试数 | 状态 |
|-----------|--------|------|
| ValueIdentityTest | 2 | ✅ PASS |
| UseDefTest | 11 | ✅ PASS |
| PrinterStabilityTest | 1 | ✅ PASS |
| OwnershipLifetimeTest | 1 | ✅ PASS |

### CFG Tests (7/7 通过)

| Test Suite | 测试数 | 状态 |
|-----------|--------|------|
| CFGBranchTest | 3 | ✅ PASS |
| CFGLoopTest | 1 | ✅ PASS |
| UnreachableBlockTest | 2 | ✅ PASS |
| CFGSmokeTest | 1 | ✅ PASS |

**总计: 22/22 tests passed (100%)**

---

## `--dump-ir-smoke` 输出

```
module {
}

func @main() -> i32 {
  local %x.0
main.entry.0:
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

覆盖的指令：Alloca, LoadImm, Store, Load, Compare, CondBranch, BinaryOp (Add), Return。包含两个以上基本块。

---

## 已实现边界

- [x] Java IR 类型完整审计（全部 15 个 Java 源文件直接读取）
- [x] `Value` 层次结构：Temp / LocalVar / GlobalVar / Constant / Label
- [x] 14 种指令类型全覆盖（Alloca → GlobalAddr）
- [x] 值指针身份（与 Java 对象身份对应）
- [x] Arena 所有权模型（Function/Module 级 value 池）
- [x] BasicBlock 含 phi/list/terminator，`all_instrs()` 合并遍历
- [x] `CFG::build()` — successor/predecessor 解析 + reachability
- [x] IR Printer — 开关分派，稳定输出排序
- [x] `build/toycc.exe --dump-ir-smoke` 驱动
- [x] Value identity 测试（同名不同址）
- [x] 全部 11 种指令 use-def 测试
- [x] CFG 三场景测试（分支/循环/不可达）
- [x] Printer 稳定性测试（两次打印一致）
- [x] 所有权生命周期测试（析构无错误）
- [x] `git diff --check` 通过
- [x] `toy-c-compiler-master` 未修改
- [x] `src2` 未修改

## 未实现边界（刻意推迟至后续阶段）

- [ ] AST → IR builder（Java `IRBuilder` 移植）
- [ ] 语义分析（`SemanticAnalyzer`）
- [ ] Lexer / Parser（现有 `src2` 或 ANTLR 方案）
- [ ] SSA 构造（Mem2Reg）
- [ ] 任何优化 pass（DCE, CSE, LICM, SCCP 等）
- [ ] Out-of-SSA（PhiLowerer）
- [ ] MIR / 寄存器分配
- [ ] RV32 汇编发射
- [ ] OJ 提交

## 下一阶段唯一目标

```
实现最小 Toy-C 前端桥接：
  int main(){ return <整数常量表达式>; }
  → Java 风格 C++ IR
  → RV32IM 汇编
  → WSL Spike
```
