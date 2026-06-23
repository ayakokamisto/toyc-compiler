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
- **Driver**: 编译器入口已集成 Lexer、Parser 和 Parser Diagnostic，支持 `-opt` 和 `--dump-tokens` 参数

Lexer, Parser, Sema, contract IR generation, IR verification, and the RISC-V32 backend are implemented as module-level components. Parser produces an `ast::CompUnit`; Sema produces a `sema::SemanticModel`; member three's IR generator produces `codegen::contract::IRModule` for the backend.

### 开发中 / In Development

- **Driver 端到端接线**: 待接入 `Sema -> ContractIRGenerator -> verifyContractModule -> RiscvBackend`
- **端到端测试升级**: 当前 driver 集成测试仍按 parser-only 行为检查，后续需升级为汇编输出/执行验证
- **IR 文档收敛**: 当前后端输入为 `ContractIR`；`src/ir/ir.h` 的旧结构 IR 仍保留但不是当前后端入口
- **优化边界**: `-opt` 已由后端消费；如需 IR 层优化，需与后端优化边界继续协调
- **实践报告**: 待编写

The main remaining integration task is connecting the completed frontend/Sema/IR/backend modules inside `src/driver/main.cpp`. Interface contracts are defined in `docs/contracts/`.

### 当前行为 / Current Behavior

- Parser 出现 Error 级语法诊断时，Driver 输出诊断并停止；Sema、IR 和 CodeGen 尚未接入 Driver 主流程
- Parser 成功后 Driver 目前仍静默返回 0，stdout 保留给后续 RISC-V32 汇编输出
- Sema、Contract IR 生成、IR 验证器和 CodeGen 已有独立库/测试，但尚未接入 Driver 主流程
- 语义分析输入边界见 `docs/contracts/sema-input-contract.md`
- IR/后端边界见 `docs/contracts/ir-contract.md` 与 `docs/contracts/ir_backend_contract.md`
- 集成回归规格位于 `tests/integration/cases/`

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

### 使用示例 / Usage Examples

编译 ToyC 源文件（当前仅解析，成功返回 0）:

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

开启优化（参数已保留，端到端接线后传递给后端）:

```bash
echo "int main() { return 0; }" | ./build/toycc -opt
```

### 输出约定 / Output Convention

- **stdout**: Driver 端到端接线后输出 RISC-V32 汇编；当前 parser-only 路径保持为空
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
│   ├── RiscvBackend        后端入口
│   ├── RiscvEmitter        汇编指令发射
│   ├── StackFrame          栈帧布局
│   ├── CallingConvention   调用约定
│   ├── InstructionSelector IR→RISC-V 指令选择
│   ├── VRegCollector       虚拟寄存器收集
│   ├── FunctionEmitter     函数体发射
│   └── ContractIR          后端自有 IR 中间表示
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
    ├── cases/           ToyC 测试用例（.tc 文件，12 组）
    ├── driver/          Driver 集成测试
    ├── codegen_snapshot_tests.cpp
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
| M4 | RISC-V32 CodeGen | ✅ 后端模块已完成，待 Driver 接线 |
| M5 | 端到端功能测试 | ⏳ 待 Driver 升级后接入 |
| M6 | `-opt` 性能优化 | 🔧 后端已有优化路径，IR 层优化待协调 |

### 编译器主链路 / Compiler Pipeline

```text
stdin → Lexer → Parser → Sema → ContractIRGenerator → verifyContractModule → CodeGen → stdout
         ✅       ✅      ✅            ✅                    ✅              ✅
```

当前 Driver 主流程范围仍是：Lexer → Parser → Driver（语法分析完成后返回 0）。Sema、IR 和 CodeGen 模块已完成，但还未接入 Driver 端到端输出。

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
