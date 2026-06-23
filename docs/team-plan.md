# ToyC 团队协作计划

## 成员职责

| 成员 | 职责 |
|---|---|
| 成员一 | Lexer、TokenStream、AST、Parser、前端测试、公共文件集成 |
| 成员二 | Sema、Symbol、Scope、常量表达式求值、语义诊断 |
| 成员三 | IR 数据结构、CFG、IRGenerator、IR Verifier |
| 成员四 | RISC-V32 CodeGen、后端优化、端到端汇编执行测试 |

成员四可从开发初期使用手写 IR 用例验证后端接口、调用约定和栈帧方案。

## Driver 集成主链路

编译器主链路固定为：

```text
stdin -> Lexer -> Parser -> Sema -> IRGenerator -> verifyModule -> CodeGen -> stdout
```

成员一已完成 Lexer、Parser 和 Parser Diagnostic 的 Driver 接入。成员二交付 Sema
公开入口后，成员一负责将 Parser 成功产生的 `ast::CompUnit` 接入
`Sema::analyze(...)`。Driver 只在 Parser 与 Sema 均成功后调用 IRGenerator，并只在
`verifyModule()` 成功后调用 CodeGen。

## main 单分支协作规则

仓库仅使用 `main` 分支。`main` 是唯一共享开发线、唯一集成线和最终提交线。
所有成员在本地 `main` 分支完成各自目录范围内的开发，并将通过验证的小型提交
依次推送到 `origin/main`。

每次开始开发或接受新任务前执行：

```bash
git switch main
git pull --ff-only origin main
git status --short
```

开发开始前，工作区应处于干净状态，并完成 `origin/main` 同步。每位成员优先在
自己负责的目录内完成完整功能与单元测试。

## 推送前验证

```bash
git diff --check
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
git status --short
```

每个提交保持单一职责。实现、对应测试和必要契约文档共同构成完整变更单元。
公共接口改动使用独立提交。推送前执行构建和完整 CTest。推送顺序由团队即时
协调，确保共享公共文件的修改按顺序集成。

## 目录所有权

| 范围 | 主要负责人 | 协作规则 |
|---|---|---|
| `src/common/token.h`、`src/common/token_stream.h`、`src/common/diagnostic.h` | 成员一 | 公共前端数据结构；修改前同步团队 |
| `src/lexer/`、`tests/frontend/` | 成员一 | 词法、TokenStream、前端测试 |
| `src/ast/`、`src/parser/` | 成员一 | AST、递归下降 Parser、Parser 测试 |
| `src/sema/`、`tests/unit/sema_*` | 成员二 | 符号表、作用域、常量表达式、语义诊断 |
| `src/ir/`、`tests/unit/ir_*` | 成员三 | IR、CFG、IRGenerator、Verifier、IR 单元测试 |
| `src/codegen/`、`tests/unit/codegen_*` | 成员四 | RISC-V32 后端、寄存器与栈帧、后端优化 |
| `tests/integration/` | 成员四主责，全体提供用例 | 端到端用例、汇编执行验证、性能回归 |
| `docs/contracts/` | 成员一维护格式，全体共同确认内容 | 接口变更与头文件改动保持同步 |
| `CMakeLists.txt`、`src/driver/main.cpp`、`README.md` | 成员一负责集成 | 其他成员修改前与成员一协调 |

## 公共文件修改规则

高冲突区域：

```text
CMakeLists.txt
src/driver/main.cpp
src/common/diagnostic.h
src/ast/ast.h
src/ir/ir.h
docs/contracts/
README.md
```

1. 公共文件修改先在团队沟通中说明修改目的、受影响模块和验证方式。
2. 公共头文件与对应 `docs/contracts/` 文件在同一提交中更新。
3. 公共接口提交完成后，其他成员以最新 `main` 为基线继续开发。
4. 接口冻结后的结构性变更附带兼容性说明、迁移步骤和测试更新。
5. 共享文件由目录负责人完成最终集成修改。

## 推荐提交粒度

```text
feat(parser): add AST declarations and parser skeleton
feat(parser): parse declarations and function definitions
feat(parser): parse expressions with precedence
feat(sema): add symbol table and nested scopes
feat(sema): validate declarations and function calls
feat(ir): add core IR data model and verifier
feat(ir): lower expressions and control flow to CFG
feat(codegen): emit RV32 function prologue and return
feat(codegen): lower arithmetic and branch instructions
test(integration): add global variable and recursion cases
docs(contract): freeze semantic model interface
```

一个提交只处理一个可验证目标。实现、测试和必要文档共同构成一个完整变更单元。

## main 集成里程碑

| 里程碑 | 内容 | main 集成条件 |
|---|---|---|
| M1 | Lexer + AST + Parser | Parser 单元测试通过；Lexer 全量回归通过；AST 契约无待决项 |
| M2 | Sema | Sema 单元测试通过；语义诊断覆盖作用域、常量、函数、break/continue、return |
| M3 | IR + CFG | IR Verifier 通过；控制流与短路逻辑 IR 测试通过 |
| M4 | RISC-V32 CodeGen | 最小 ToyC 程序可编译为可运行 RV32 汇编；main 返回值正确 |
| M5 | 端到端功能测试 | 全局变量、常量、函数调用、递归、if/while、短路逻辑端到端测试通过 |
| M6 | `-opt` 性能优化 | `-opt` 回归通过；优化前后结果一致；性能用例具备基准记录 |

## 测试与交付约束

- 前端测试：`tests/frontend/<module>_<behavior>_tests.cpp`
- 单元测试：`tests/unit/<module>_<behavior>_tests.cpp`
- 集成源码：`tests/integration/cases/<feature>.tc`
- 集成期望：`tests/integration/cases/<feature>.expected`
- 生成的汇编、目标文件和可执行文件统一放入 `build/`。
- 项目使用 C++20 和 CMake，目标输出为 RISC-V32 汇编。
- 功能评测时间限制为1秒，性能评测时间限制为20秒。
- 性能基准采用 gcc `-O2`，程序结果以 `main` 返回值为准。
- 项目实现使用自有后端基础设施。

# Current Driver Integration Status

- Driver full-chain wiring is complete in `src/driver/main.cpp`.
- The default path is `stdin -> Lexer -> Parser -> Sema -> Contract IR -> IR Verify -> RISC-V32 CodeGen -> stdout`.
- M5 is now in the RISC-V toolchain stage: assemble, link, execute, and compare process exit codes.
- The current local Windows environment needs RISC-V GCC and QEMU to run generated assembly end to end.
