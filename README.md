# ToyC Compiler

ToyC 是 C 语言的简化子集。本项目是一个面向 RISC-V32 汇编的 ToyC 编译器，使用 C++20 实现。

ToyC is a simplified subset of C. This project is a ToyC compiler targeting RISC-V32 assembly, implemented in C++20.

## 项目状态 / Project Status

### 已完成 / Completed

- **Lexer**: 完整实现 ToyC 词法分析，支持所有关键字、运算符、标识符、整数字面量、注释
- **TokenStream**: Token 流封装，支持 peek/consume/expect/match 操作
- **AST**: 完整的抽象语法树节点定义，覆盖 ToyC 所有语法结构
- **Parser**: 递归下降语法分析器，实现全部 ToyC 文法产生式，含错误恢复机制
- **Sema**: 语义分析已实现，覆盖符号表、嵌套作用域、常量求值、函数签名、返回检查、`break`/`continue` 合法性
- **IR**: 成员三交付的 `ContractIRGenerator` 已实现，可将 AST + `SemanticModel` 降低为后端契约 IR，并提供结构验证器
- **CodeGen**: RISC-V32 后端已实现，覆盖栈帧、调用约定、指令选择、汇编发射、寄存器分配和后端优化路径
- **Driver**: 编译器入口已全链路接线（Lexer → Parser → Sema → Contract IR → IR Verify → RISC-V32 CodeGen），支持 `-opt` 和 `--dump-tokens` 参数

Lexer, Parser, Sema, contract IR generation, IR verification, and the RISC-V32 backend are wired through the default `toycc` compilation path. Parser produces an `ast::CompUnit`; Sema produces a `sema::SemanticModel`; member three's IR generator produces `codegen::contract::IRModule` for the backend.

### 开发中 / In Development

- **实践报告**: 待最终整理
- **后续优化**: 当前 `-opt` 已完成课程项目第一版优化；更复杂的 interval splitting、copy coalescing、IR 层 CSE/DCE 可作为后续增强，不作为当前交付阻塞项

The default `toycc` path now connects the completed frontend/Sema/IR/backend modules inside `src/driver/main.cpp`. Interface contracts are defined in `docs/contracts/`. End-to-end RISC-V execution tests are available through an opt-in CTest target when the RISC-V toolchain and QEMU are installed.

### 当前行为 / Current Behavior

- 默认路径：`stdin → Lexer → Parser → Sema → Contract IR → IR Verify → RISC-V32 CodeGen → stdout`
- 成功：完整汇编输出到 stdout，exit 0
- 失败：诊断信息输出到 stderr，stdout 为空，exit 1
- `--dump-tokens`：Lexer 后输出 token 到 stderr 并返回 0，不进入后续阶段
- `-opt`：传递给后端 `BackendOptions.enableOpt`，启用后端优化路径
- 语义分析输入边界见 `docs/contracts/sema-input-contract.md`
- IR/后端边界见 `docs/contracts/ir-contract.md` 与 `docs/contracts/ir_backend_contract.md`
- 集成回归规格位于 `tests/integration/cases/`

当前已验证：

- 默认 CTest：46/46 通过
- 开启 `TOYC_ENABLE_RISCV_EXEC_TESTS=ON` 后：47/47 通过
- ToyC 源码级执行闭环：17 个 case 的 default/`-opt` 共 34/34 通过
- `-opt` 度量基线：17/17 case 的 `lw` 下降，汇总 `lw` 从 350 降到 238，汇总 `sw` 从 330 降到 304，`lw+sw increases: none`

## 构建与测试 / Build and Test

### 环境要求 / Requirements

- C++20 编译器（GCC 10+、Clang 11+、MSVC 19.29+）
- CMake 3.20+

### 构建命令 / Build Commands

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Windows MSYS2/MinGW 环境需指定生成器 / On Windows with MSYS2/MinGW, specify the generator:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
```

推荐的 Ninja 验证命令 / Recommended Ninja validation:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

RISC-V 执行验证需要额外安装 `riscv64-linux-gnu-gcc` 与 `qemu-riscv32`。在 WSL/Unix 环境中可开启可选 CTest：

```bash
cmake -S . -B build -G Ninja -DTOYC_ENABLE_RISCV_EXEC_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

该模式会注册 `toycc_riscv_exec_cases`，对 `tests/integration/cases/*.tc` 执行 default 和 `-opt` 两种编译模式，汇编、链接、运行并比较 `.expected` 中的 `main` 退出码。

### 使用示例 / Usage Examples

编译 ToyC 源文件（全链路编译，输出 RISC-V32 汇编）:

```bash
# Unix
echo "int main() { return 42; }" | ./build/toycc

# Windows PowerShell
Get-Content program.tc | .\build\toycc.exe
```

查看 Token 流 / Token dump:

```bash
# Unix
echo "int main() { return 0; }" | ./build/toycc --dump-tokens

# Windows PowerShell
Get-Content program.tc | .\build\toycc.exe --dump-tokens
```

开启优化（传递给后端启用后端优化路径）:

```bash
echo "int main() { return 0; }" | ./build/toycc -opt
```

源码级执行闭环也可以手动运行：

```bash
bash tests/integration/run_toyc_exec_cases.sh --build-dir build
python3 tests/integration/measure_codegen_opt.py --build-dir build
```

`run_toyc_exec_cases.sh` 会为每个 case 和模式保存 toycc、RISC-V GCC、QEMU
的命令、stdout、stderr 和退出码日志，并用 `--timeout` 控制单条命令的最大运行时间。

### 输出约定 / Output Convention

- **stdout**: 输出 RISC-V32 汇编（成功时）；编译失败时为空
- **stderr**: Token dump、诊断信息
- **退出码**: 0=成功，1=编译错误，2=参数错误

## 目录结构 / Project Structure

```text
src/
├── common/          公共数据结构（Token、Diagnostic）
│   ├── token.h          Token 定义和 TokenKind 枚举
│   ├── token_stream.h   Token 流封装
│   └── diagnostic.h     诊断信息结构
├── lexer/           词法分析器
│   ├── lexer.h          Lexer 类声明
│   └── lexer.cpp        Lexer 实现
├── parser/          语法分析器（递归下降）
│   ├── parser.h         Parser 类声明
│   └── parser.cpp       Parser 实现
├── ast/             抽象语法树节点定义
│   └── ast.h            所有 AST 节点类型
├── ir/              Contract IR 生成与结构验证
│   ├── contract_ir_generator.h  AST/Sema → 后端契约 IR 入口
│   ├── contract_ir_generator.cpp IR lowering 与 verifier 实现
│   └── ir.h             旧结构 IR 声明（当前非后端输入）
├── sema/            语义分析（符号表、作用域、常量求值）
├── codegen/         RISC-V32 代码生成
│   ├── RiscvBackend.*      后端入口
│   ├── ContractIR.h        后端消费的契约 IR 视图
│   ├── emit/               汇编文本发射
│   ├── frame/              栈帧、vreg 分析、linear-scan 分配
│   ├── abi/                RISC-V32 调用约定
│   ├── lower/              函数发射、指令选择、branch fusion、vreg cache
│   └── opt/                peephole 优化
└── driver/          编译器入口
    └── main.cpp         主程序入口和命令行参数处理
tests/
├── frontend/        前端测试（Lexer、TokenStream、Parser）
│   ├── lexer_tests.cpp
│   ├── token_stream_tests.cpp
│   └── parser_p*_tests.cpp   Parser 分阶段测试（P1-P5）
├── unit/            各模块单元测试
│   ├── codegen_stack_frame_tests.cpp
│   ├── codegen_backend_smoke_tests.cpp
│   ├── codegen_calling_convention_tests.cpp
│   ├── ir_contract_generator_tests.cpp
│   ├── sema_analysis_tests.cpp
│   └── codegen_vreg_collector_tests.cpp
└── integration/     端到端集成测试
    ├── cases/           ToyC 测试用例（.tc 文件，17 组）
    ├── driver/          Driver 集成测试
    ├── codegen_snapshot_tests.cpp
    ├── run_toyc_exec_cases.sh
    ├── measure_codegen_opt.py
    └── driver_test.cmake
docs/
├── contracts/       接口契约文档
│   ├── frontend-contract.md    Token、Lexer、TokenStream 接口
│   ├── ast-contract.md         AST 节点层次和所有权模型
│   ├── parser-contract.md      Parser 入口和文法决策
│   ├── sema-input-contract.md  Sema 输入边界和职责
│   ├── ir-contract.md          当前 IR/后端边界
│   └── ir_backend_contract.md  成员三到成员四的详细后端契约
├── team-plan.md     团队协作计划
└── report/          实践报告 [待编写]
```

## 接口契约 / Public Contracts

| 文档 | 内容 | 状态 |
|------|------|------|
| [frontend-contract.md](docs/contracts/frontend-contract.md) | Token 定义、Lexer 行为、TokenStream 接口 | ✅ 已冻结 |
| [ast-contract.md](docs/contracts/ast-contract.md) | AST 节点层次、所有权模型、语法-语义边界 | ✅ 已冻结 |
| [parser-contract.md](docs/contracts/parser-contract.md) | Parser 入口、文法决策、诊断与恢复策略 | ✅ 已冻结 |
| [sema-input-contract.md](docs/contracts/sema-input-contract.md) | Parser AST 到 Sema 的输入边界与职责 | ✅ 已定义 |
| [ir-contract.md](docs/contracts/ir-contract.md) | 当前 IR/后端边界、ContractIRGenerator 与 verifier 约束 | ✅ 已定义 |
| [ir_backend_contract.md](docs/contracts/ir_backend_contract.md) | 成员三到成员四的后端契约 IR 详细字段与指令约定 | ✅ 已定义 |

接口变更需与对应头文件和契约文档同步提交。

Interface changes must be submitted together with the corresponding header and contract document.

## 团队协作 / Collaboration

- 仓库使用 `main` 作为唯一共享分支
- 开发前执行 `git pull --ff-only origin main`
- 每个提交保持单一职责，附带测试和必要文档
- 公共接口定义遵循 `docs/contracts/`
- 团队分工与集成规则见 [team-plan.md](docs/team-plan.md)
- 高冲突文件（`CMakeLists.txt`、`src/common/diagnostic.h`、`src/ast/ast.h`、`src/ir/ir.h`）修改前需团队协调

### 里程碑进度 / Milestone Progress

| 里程碑 | 内容 | 状态 |
|--------|------|------|
| M1 | Lexer + AST + Parser | ✅ 已完成 |
| M2 | Sema（语义分析） | ✅ 已完成模块实现与单元测试 |
| M3 | IR + CFG（中间表示） | ✅ Contract IR 生成器与 verifier 已完成模块实现 |
| M4 | RISC-V32 CodeGen | ✅ 已接入 Driver 全链路 |
| M5 | 端到端功能测试 | ✅ 已完成源码级 assemble/link/run/exit-code 验证，并可选接入 CTest |
| M6 | `-opt` 性能优化 | ✅ 后端优化第一版已完成，具备度量脚本与基线数据 |

### 编译器主链路 / Compiler Pipeline

```text
stdin → Lexer → Parser → Sema → ContractIRGenerator → verifyContractModule → CodeGen → stdout
         ✅       ✅      ✅            ✅                    ✅              ✅
```

Driver 全链路已接线：stdin → Lexer → Parser → Sema → ContractIRGenerator → verifyContractModule → RISC-V32 CodeGen → stdout。成功输出汇编并 exit 0；失败输出诊断到 stderr 并 exit 1。开启 `TOYC_ENABLE_RISCV_EXEC_TESTS=ON` 后，CTest 会额外运行 RISC-V GCC + QEMU 执行闭环，按 `.expected` 比较 `main` 返回值。

---

## ToyC 语言快速参考 / ToyC Language Quick Reference

### 数据类型 / Data Types

- `int` — 32 位有符号整数
- `void` — 无返回值（仅用于函数）

### 变量与常量 / Variables and Constants

```c
int x = 42;           // 全局变量
const int N = 10;     // 全局常量

int main() {
    int a = 1;        // 局部变量
    const int B = 2;  // 局部常量
    return a + B;
}
```

### 函数定义 / Function Definitions

```c
int add(int a, int b) {
    return a + b;
}

void print_value(int x) {
    // void 函数可无 return
}

int main() {
    return add(1, 2);
}
```

### 控制流 / Control Flow

```c
if (x > 0) {
    // ...
} else {
    // ...
}

while (i < 10) {
    if (i == 5) break;
    if (i == 3) { i++; continue; }
    i++;
}
```

### 运算符 / Operators

| 类别 | 运算符 |
|------|--------|
| 算术 | `+` `-` `*` `/` `%` |
| 关系 | `<` `>` `<=` `>=` `==` `!=` |
| 逻辑 | `&&` `\|\|` `!` |
| 赋值 | `=` |

### 完整示例 / Complete Example

```c
// 计算斐波那契数列第 N 项
const int N = 10;

int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    return fib(N);  // 返回 55
}
```

### 注释 / Comments

```c
// 单行注释

/* 多行
   注释 */
```
