# ToyC Compiler

ToyC 语言编译器 — 武汉大学编译原理课程实践项目。

## 目标

将 ToyC（C 的简化子集）源代码编译为可正确执行的 RISC-V32 汇编。

## 当前状态

**P3 — 语义分析器**：Lexer + Parser + AST + 语义分析已实现，支持 `--dump-tokens`、`--dump-ast`、`--dump-sema` 调试模式。
当前尚未实现 IR 生成或汇编生成功能。

## 接口

- **stdin** → ToyC 源码
- **stdout** → RISC-V32 汇编（当前未实现，输出诊断信息到 stderr）
- **stderr** → 诊断和调试信息
- **`-opt`** 参数启用优化通道（当前未实现）
- **`--dump-tokens`** 将 token 流输出到 stderr（调试用）

## 构建

```bash
cmake -S . -B build
cmake --build build -j
```

## 开启本地测试

```bash
cmake -S . -B build -DTOYC_BUILD_TESTS=ON
cmake --build build -j
./build/toyc-frontend-tests
./build/toyc-ir-tests
```

## 使用

```bash
# 显示帮助
./build/toycc --help

# 词法分析调试（token dump 到 stderr）
./build/toycc --dump-tokens < input.tc

# 语法分析调试（AST dump 到 stderr）
./build/toycc --dump-ast < input.tc

# 语义分析调试（SemanticModel dump 到 stderr）
./build/toycc --dump-sema < input.tc

# 编译（当前未实现）
./build/toycc < input.tc > output.s
```

## 模块结构

```text
toyc_support    — 公共类型、诊断
toyc_frontend   — Lexer / Parser / AST
toyc_sema       — 语义分析
toyc_ir         — SSA IR
toyc_analysis   — CFG / Dominator / Loop 分析
toyc_passes     — 优化 Pass
toyc_mir        — Machine IR
toyc_riscv32    — RISC-V32 后端
toycc           — 编译器可执行入口
```

## 文档

- [架构设计](docs/architecture.md)
- [构建与测试](docs/build-and-test.md)
- [IR 规范](docs/ir-spec.md)
- [后端规范](docs/backend-spec.md)
- [开发阶段](docs/development-phases.md)
