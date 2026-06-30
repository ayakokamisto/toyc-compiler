# src ABI repair report

## 1. 修改前风险清单

| 风险点 | 文件路径 | 函数名 | 触发条件 | 可能错误 | 建议 | 影响 |
| --- | --- | --- | --- | --- | --- | --- |
| union-find 寄存器分配 | `src/target/riscv32/reg_allocator.cpp` | `RegisterAllocator::allocate` | `opts.optimize` 启用后对 `Move` 建等价类并分配物理寄存器 | 不精确等价类合并导致覆盖仍然活跃的 VReg | 立即关闭 | 正确性提升，局部性能下降 |
| 跨块 IRSlot 转发 | `src/target/riscv32/reg_allocator.cpp` | `RegisterAllocator::allocate` | 前驱 `StoreFrame` 与后继首个 `LoadFrame` 使用同一 frame slot | 控制流合流、回边或多前驱时读到错误值 | 立即关闭 | 正确性提升，循环 `lw/sw` 增加 |
| emitter 跨 block `slotToVReg_` 状态 | `src/target/riscv32/asm_emitter.cpp` | `Emitter::cacheClear`, `Emitter::emitInstruction` | `StoreFrame` 后跨 block 复用 `slotToVReg_` | block 边界后假设寄存器仍持有 slot 值 | 立即关闭跨块保留 | 正确性提升，块内转发保留 |
| `s1-s11` 无保存恢复 | `src/target/riscv32/reg_allocator.cpp`, `src/target/riscv32/asm_emitter.cpp` | `RegisterAllocator::allocate`, `Emitter::emitFunction`, `Emitter::emitReturn` | 分配器把 VReg 放入 `s*` | 调用者 callee-saved 值被破坏 | 立即修复 | ABI 正确，frame 增加保存槽 |
| `t* / a*` 承载跨 call 活跃值 | `src/target/riscv32/reg_allocator.cpp` | `RegisterAllocator::allocate` | 非叶函数或 call 前后活跃 VReg 被分配到 caller-saved | `call` 覆盖活跃值 | 立即关闭 | 正确性提升，可用寄存器减少 |
| `VRegHome` 强制写回 | `src/target/riscv32/asm_emitter.cpp` | `Emitter::storeDestination`, `Emitter::emitBinary`, `Emitter::emitImmediate` | VReg 写入时同步写回栈槽 | 热循环 `sw` 偏多 | 保留 | 正确性优先，性能瓶颈保留 |
| BlockVRegCache 边界处理 | `src/target/riscv32/asm_emitter.cpp` | `Emitter::cacheClear`, `Emitter::emitFunction`, `Emitter::emitInstruction` | block 入口或 call 后缓存未失效 | `t0/t1` 读到过期值 | 保留块内缓存并清边界 | 正确性提升，块内冗余 load 仍可减少 |

## 2. 已关闭的高风险优化

- `src/target/riscv32/reg_allocator.cpp` 改为保守线性扫描，删除 union-find、跨块 slot forwarding、caller-saved 寄存器池。
- `src/target/riscv32/asm_emitter.cpp` 的 `slotToVReg_` 在 `cacheClear()` 中清空；`cacheClear()` 在函数入口、block 入口、call 前后执行。
- `src/driver/main.cpp` 普通编译路径固定启用安全 `RegisterAllocator(true)`，`-opt` 只继续控制 SSA/peephole 优化。

## 3. ABI 修复点

- `include/toyc/mir/mir.h` 增加 `FrameObjectKind::SavedCalleeSaved` 和 `physReg` 字段。
- `src/mir/mir.cpp` 的 `FrameLayout::compute` 为 callee-saved 保存槽分配 offset，并保持 16 字节栈对齐。
- `src/target/riscv32/reg_allocator.cpp` 线性扫描只分配 `s1-s11`，未分配 VReg 继续使用 VRegHome spill/reload。
- `src/target/riscv32/asm_emitter.cpp` 在 prologue 保存已分配的 `s*`，在统一 epilogue 反向恢复 `s*` 与 `ra`。
- 所有 MIR `Return` 统一跳转到 `<function>.epilogue`。
- 参数进入函数时同步写入 VRegHome 和已分配的 `s*`。

## 4. 新增测试列表

- `CodegenDriverTest.LinearScanSavesCalleeSavedRegistersInLeafAndNonLeaf`
- `CodegenDriverTest.CallerSavedRegistersAreOnlyScratchAroundCalls`
- `CodegenDriverTest.ReturnsUseUnifiedEpilogue`
- `CodegenDriverTest.AbiRegressionProgramsGenerateAssembly`
- `tools/src_backend_regression/cases/leaf_s_registers.tc`
- `tools/src_backend_regression/cases/nonleaf_s_registers.tc`
- `tools/src_backend_regression/cases/caller_local_after_call.tc`
- `tools/src_backend_regression/cases/loop_variable_across_call.tc`
- `tools/src_backend_regression/cases/multi_level_calls.tc`
- `tools/src_backend_regression/cases/recursive_call.tc`
- `tools/src_backend_regression/cases/if_else_merge.tc`
- `tools/src_backend_regression/cases/while_backedge.tc`

## 5. 测试结果

已提升权限执行：

| 命令 | 结果 |
| --- | --- |
| `cmake -S . -B build -DTOYC_BUILD_TESTS=ON` | pass |
| `cmake --build build -j` | pass |
| `build\toyc-frontend-tests.exe` | 194/194 pass |
| `build\toyc-ir-tests.exe` | 74/74 pass |
| `build\toyc-analysis-tests.exe` | 4/4 pass |
| `build\toyc-sema-tests.exe` | 122/122 pass |
| `build\toyc-lowering-tests.exe` | 42/42 pass |
| `build\toyc-ssa-tests.exe` | 26/26 pass |
| `build\toyc-mir-tests.exe` | 8/8 pass |
| `build\toyc-riscv32-tests.exe` | 4/4 pass |
| `build\toyc-codegen-tests.exe` | 10/10 pass |
| `powershell -ExecutionPolicy Bypass -File tools\src_backend_regression\run.ps1` | 8/8 compile pass, 8/8 static pass |

`tools/src_backend_regression/reports/latest.md`:

| case | expected | compile | static | runtime | lw | sw | call | mul | div | rem |
| --- | ---: | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| caller_local_after_call | 19 | pass | pass | skipped: no riscv runner | 8 | 18 | 1 | 0 | 0 | 0 |
| if_else_merge | 13 | pass | pass | skipped: no riscv runner | 5 | 17 | 0 | 0 | 0 | 0 |
| leaf_s_registers | 10 | pass | pass | skipped: no riscv runner | 4 | 18 | 0 | 0 | 0 | 0 |
| loop_variable_across_call | 10 | pass | pass | skipped: no riscv runner | 11 | 26 | 1 | 0 | 0 | 0 |
| multi_level_calls | 11 | pass | pass | skipped: no riscv runner | 8 | 20 | 2 | 0 | 0 | 0 |
| nonleaf_s_registers | 9 | pass | pass | skipped: no riscv runner | 8 | 18 | 1 | 0 | 0 | 0 |
| recursive_call | 120 | pass | pass | skipped: no riscv runner | 8 | 20 | 3 | 0 | 0 | 0 |
| while_backedge | 10 | pass | pass | skipped: no riscv runner | 8 | 18 | 0 | 0 | 0 | 0 |

本机缺少 `riscv32-unknown-elf-gcc` / `riscv32-linux-gnu-gcc` 与 `qemu-riscv32*`，运行时执行按脚本规则跳过。

## 6. 与 src2 和参考实现的差距

- `src2` 已拆分 `codegen/abi`, `codegen/frame`, `codegen/lower`, `codegen/emit`, `codegen/opt`，当前 `src` 已具备 MIR/allocator/emitter 基线，但 frame 与 ABI 仍集中在 MIR/emitter 内。
- `src2` 有独立 `VRegAnalysis`, `VRegAssignment`, `RegisterAllocator`, `StackFrame`, `BlockVRegCache`，当前 `src` 的线性扫描采用保守全函数区间，缺少块级 liveness 与更细粒度 spill placement。
- `toy-c-compiler-master` 的 Java emitter 可作为指令选择和简单发射参考；本次未移植其中端或性能优化。
- 当前 `src` 以 ABI 正确和静态可验证为优先，热循环仍保留大量 VRegHome `lw/sw`。

## 7. 保留的性能瓶颈

- 每次 VReg 定义仍同步写回 VRegHome，`sw` 数量偏高。
- `LoadFrame`/`StoreFrame` 跨块转发已关闭，循环 header 和回边会保留栈流量。
- 线性扫描只使用 `s1-s11`，未使用 leaf-only caller-saved 池。
- spill/reload placement 仍由 emitter fallback 触发，缺少显式 spill slot 合并和 rematerialization。

## 8. 下一步单一优化目标

下一步只做块内 VRegHome 写回抑制：在单 basic block 内基于 def-use 顺序跳过死写回，保持 block 边界、call 边界、branch 边界强制落栈。
