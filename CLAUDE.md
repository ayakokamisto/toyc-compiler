# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目性质

武汉大学编译原理课程实践：四人为组实现 **ToyC 语言编译器**，将 ToyC 源代码编译为可正确执行的 **RISC-V32 汇编**。最终由在线评测系统（OJ）自动克隆、构建、评测打分。

**权威需求文档是 `任务要求.md`**（中文，包含完整文法与评分公式）。任何关于语言定义、接口、评分细则的疑问，以该文件为准。

## 文档优先级

当各文档内容冲突时，按以下顺序取信：

1. `任务要求.md` — 语言定义与评分规则的最终权威
2. `docs/architecture.md` — 架构和设计决策
3. `AGENTS.md` — 项目约束和 agent 规则
4. `CLAUDE.md` — 工具链和构建说明
5. `词汇翻译表.md` — 词法/语法 Token 参考

## 接口契约（对评测至关重要，不可破坏）

- 编译器从 **stdin** 读 ToyC 源程序，向 **stdout** 输出 RISC-V32 汇编，stderr 输出诊断和调试信息。
- 本地测试：`toycc < input.tc > output.s`
- 性能测试时评测器会传入 `-opt` 参数表示开启优化；可据此启用优化 pass，也可忽略。
- 程序运行结果以 **`main` 函数的返回值**（退出码，0~255）为准——RV32 后端将 main 返回值置入 `a0`，通过 `ecall` 退出。

## 实现约束（评测环境版本）

- 语言：**C++20**，构建系统 **CMake (4.0.3)**。
  - 项目根目录必须包含 `CMakeLists.txt`。
- 最终可执行文件名：**`toycc`**。
- 允许的第三方库：Flex、Bison/Yacc、ANTLR (4.13.1)。
- **禁止**：抄袭、套用 Clang/LLVM 等现成后端框架。

> 评测系统会自动探测「编程语言 / 项目结构 / 构建系统 / 可执行名或主类名」，因此语言选定后构建配置需符合常规约定。

## 编译器架构主线

```text
Source (stdin)
→ Lexer / Parser / AST
→ SemanticModel
→ Canonical Slot IR
→ Mem2Reg
→ Optimizing SSA IR
→ Out-of-SSA
→ RV32 MIR
→ Register Allocation / Frame Layout
→ RISC-V32 Assembly (stdout)
```

## ToyC 语言要点（C 的简化子集）

- 相比 C **去掉**：数组、指针、I/O、多文件编译。
- **本年度新增**：全局变量、全局常量。
- 所有声明（`VarDecl` / `ConstDecl`）**必须带初始化表达式**；`const` 初始化式只能含数字字面量、已声明常量及其算术/逻辑组合。
- 入口必须是 `int main()` 无参。
- ToyC 源码可用普通 C 编译器直接编译，运行结果与等价 C 代码一致——**这是本地验证生成汇编正确性的有效手段**。

## 评分（影响优化投入的权衡）

- 总分 = 评测分 × 80% + 实践报告 × 20%。
- 评测分 = 功能分 × 75% + 性能分 × 25%。
- 性能分以 `gcc -O2` 生成代码运行时间为基准（`min(1, 基准/实际)` 封顶），故后端优化收益明显。
- 时间限制：功能测试 1s，性能测试 20s（生成代码运行时限，非编译时限；编译时间宽松、编译器自身效率不影响评分）。

## 开发环境 / 工具链

- 已确定使用 **C++20**，构建系统 **CMake 4.0.3**。
- 本地开发若缺少 CMake，按以下方式自动安装（与 OJ 版本对齐）：
  ```bash
  pip3 install cmake==4.0.3
  ```
  安装后确保 `cmake --version` 显示 `4.0.3`。
- 构建命令统一使用：
  ```bash
  cmake -S . -B build -DTOYC_BUILD_TESTS=ON
  cmake --build build -j
  ./build/toyc-frontend-tests
  ./build/toyc-ir-tests
  ```
- **测试框架**：GoogleTest（gtest_main），不使用 CTest。测试二进制直接运行。

## 开发流程
对照本 CLAUDE.md 和 docs 中的设计，领取分工，之后开始按模块实现。
