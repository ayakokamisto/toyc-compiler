# ToyC Compiler

ToyC 是 C 语言的简化子集。本项目是一个面向 RISC-V32 汇编的 ToyC 编译器，使用 C++20 实现。

ToyC is a simplified subset of C. This project is a ToyC compiler targeting RISC-V32 assembly, implemented in C++20.

## 项目状态 / Project Status

当前已完成 Lexer、TokenStream，以及 ToyC 全部语法产生式的 Parser。Parser 输出为
`ast::CompUnit`。Sema、IR 生成和 CodeGen 正在分阶段开发中。

Lexer, TokenStream, and the complete ToyC Parser are implemented. Parser produces an
`ast::CompUnit`. Sema, IR generation, and CodeGen are under staged development.

Parser 出现 Error 级语法诊断时，Driver 应输出诊断并停止进入 Sema、IR 和 CodeGen。
语义分析输入边界见 `docs/contracts/sema-input-contract.md`。

## 构建与测试 / Build and Test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Windows MSYS2/MinGW 环境需指定生成器 / On Windows with MSYS2/MinGW, specify the generator:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
```

Token 查看示例 / Token dump examples:

```bash
# Unix
echo "int main() { return 0; }" | ./build/toycc --dump-tokens

# Windows PowerShell
Get-Content program.tc | .\build\toycc.exe --dump-tokens
```

Token 输出和诊断信息写入 stderr，stdout 保留给生成的汇编代码。

Token dumps and diagnostics use stderr. stdout is reserved for generated assembly.

## 目录结构 / Project Structure

```text
src/
├── common/          公共数据结构（Token、Diagnostic）
├── lexer/           词法分析器
├── parser/          语法分析器（递归下降）
├── ast/             抽象语法树节点定义
├── sema/            语义分析（符号表、作用域、常量求值）
├── ir/              中间表示与控制流图
├── codegen/         RISC-V32 代码生成
└── driver/          编译器入口
tests/
├── frontend/        前端测试（Lexer、TokenStream、Parser）
├── unit/            各模块单元测试
└── integration/     端到端集成测试
docs/
├── contracts/       接口契约文档
└── report/          实践报告
```

## 接口契约 / Public Contracts

| 文档 | 内容 |
|------|------|
| [frontend-contract.md](docs/contracts/frontend-contract.md) | Token 定义、Lexer 行为、TokenStream 接口 |
| [ast-contract.md](docs/contracts/ast-contract.md) | AST 节点层次、所有权模型、语法-语义边界 |
| [parser-contract.md](docs/contracts/parser-contract.md) | Parser 入口、文法决策、诊断与恢复策略 |
| [sema-input-contract.md](docs/contracts/sema-input-contract.md) | Parser AST 到 Sema 的输入边界与职责 |
| [ir-contract.md](docs/contracts/ir-contract.md) | IR 指令集、CFG 降低规则、验证器约束 |

接口变更需与对应头文件和契约文档同步提交。

Interface changes must be submitted together with the corresponding header and contract document.

## 团队协作 / Collaboration

- 仓库使用 `main` 作为唯一共享分支
- 开发前执行 `git pull --ff-only origin main`
- 每个提交保持单一职责，附带测试和必要文档
- 公共接口定义遵循 `docs/contracts/`
- 团队分工与集成规则见 [team-plan.md](docs/team-plan.md)
- 高冲突文件（`CMakeLists.txt`、`src/common/diagnostic.h`、`src/ast/ast.h`、`src/ir/ir.h`）修改前需团队协调
