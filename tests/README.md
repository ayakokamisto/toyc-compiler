# Tests Layout

本文件定义 **ToyC 编译器测试目录规范**。新增或移动测试时请遵守；`docs/team-plan.md` 中的模块所有权以本结构为准。

## 目录总览

```text
tests/
├── README.md                 # 本规范（全队必读）
├── frontend/                 # 前端单元测试（成员一）
│   ├── lexer_tests.cpp
│   ├── token_stream_tests.cpp
│   ├── parser_p1_tests.cpp … parser_p5_tests.cpp
│   └── parser_test_helpers.h # 仅 frontend 内共享的头文件
├── unit/                     # 各模块单元测试（成员二/三/四）
│   ├── codegen_*_tests.cpp   # 成员四：后端模块级单测
│   ├── sema_*_tests.cpp      # 成员二：待添加
│   └── ir_*_tests.cpp        # 成员三：待添加
└── integration/              # 跨模块 / 端到端（见 integration/README.md）
    ├── driver_test.cmake     # Driver 集成测试的 CMake 驱动脚本
    ├── driver/               # Driver 壳层用例（.tc 输入）
    ├── cases/                # 端到端 ToyC 规格（.tc + .expected）
    └── codegen/              # 后端集成（手写契约 IR，不依赖前端）
        └── snapshot_tests.cpp
```

## 命名规则

| 层级 | 路径模式 | 示例 | 注册方式 |
|---|---|---|---|
| 前端 | `tests/frontend/<topic>_tests.cpp` | `parser_p3_tests.cpp` | `add_executable` + `add_test` |
| 单元 | `tests/unit/<module>_<feature>_tests.cpp` | `codegen_peephole_tests.cpp` | 同上 |
| 集成（可执行） | `tests/integration/<area>/<name>_tests.cpp` | `codegen/snapshot_tests.cpp` | CTest 名建议保留 `codegen_snapshot_tests` 等稳定前缀 |
| 集成（数据） | `tests/integration/cases/<feature>.tc` | `recursion.tc` | `driver_test.cmake` 或未来 e2e 脚本 |
| 期望退出码 | `tests/integration/cases/<feature>.expected` | 单行整数 0–255 | 与同名 `.tc` 配对 |

**禁止**：

- 在 `tests/` 根目录直接放 `*_tests.cpp`
- 把模块单元测试放进 `integration/`（集成只放跨模块或端到端）
- 在后端单元测试里依赖 AST / Parser（后端只测契约 IR）

## 各目录职责

### `tests/frontend/`

- **所有者**：成员一
- **内容**：Lexer、TokenStream、Parser 分阶段回归
- **依赖**：`toyc_frontend`

### `tests/unit/`

- **所有者**：按模块前缀 — `sema_*` 成员二，`ir_*` 成员三，`codegen_*` 成员四
- **内容**：单模块、可独立链接的最小正确性测试
- **依赖**：对应 `toyc_*` 静态库

### `tests/integration/`

- **所有者**：成员四主责编排；**用例数据**（`cases/`）全队共建
- **`driver/`**：Driver 行为（退出码、stderr 诊断、`--dump-tokens` 等）
- **`cases/`**：ToyC 源程序 + 期望 `main` 退出码；Parser 已接入；Sema/IR/CodeGen 就绪后逐阶段启用
- **`codegen/`**：成员四手写 `contract::IRModule` 的汇编快照，验证后端语义

## CMake 约定

- 测试可执行文件命名：与 CTest `NAME` 一致，如 `codegen_stack_frame_tests`
- 集成测试源码路径变更时，只改 `CMakeLists.txt` 中 `add_executable` 路径，**CTest 名称保持不变**，避免 CI/脚本失效
- 全量回归：`cmake --build build` 后 `ctest --test-dir build --output-on-failure`
- 仅跑后端：`ctest --test-dir build --output-on-failure -R "^codegen_"`
