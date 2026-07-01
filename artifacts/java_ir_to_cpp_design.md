# Java IR → C++ IR 移植设计文档

## 审计来源

所有结论基于以下 Java 源文件（位于 `toy-c-compiler-master/src/main/java/toyc/`）：

### IR 核心包

| 文件路径 | 类 | 职责 |
|---|---|---|
| `ir/IRProgram.java` | `IRProgram` | 顶层 IR 容器，持有 Module |
| `ir/IRBuilder.java` | `IRBuilder` | AST→IR 降级器（暂不移植） |
| `ir/IRPrinter.java` | `IRPrinter` | IR 文本打印 |
| `ir/IRVisitor.java` | `IRVisitor<R,C>` | Visitor 接口 |
| `ir/IRTraversalVisitor.java` | `IRTraversalVisitor<C>` | 默认遍历骨架 |
| `ir/value/IRValue.java` | `IRValue` | 值基类（Type + name） |
| `ir/value/Temp.java` | `Temp` | 临时值，`%tN` |
| `ir/value/LocalVar.java` | `LocalVar` | 局部变量槽，`%name.N` |
| `ir/value/GlobalVar.java` | `GlobalVar` | 全局变量，`@name` |
| `ir/value/Constant.java` | `Constant` | 整型常量 |
| `ir/value/Label.java` | `Label` | 基本块标签 |
| `ir/inst/Instruction.java` | `Instruction` | 指令基类 |
| `ir/inst/Alloca.java` | `Alloca` | 栈分配 |
| `ir/inst/Load.java` | `Load` | 内存读取 |
| `ir/inst/Store.java` | `Store` | 内存写入 |
| `ir/inst/LoadImm.java` | `LoadImm` | 加载立即数 |
| `ir/inst/BinaryOp.java` | `BinaryOp` | 二元算术运算 |
| `ir/inst/UnaryOp.java` | `UnaryOp` | 一元运算 |
| `ir/inst/Compare.java` | `Compare` | 比较运算 |
| `ir/inst/Call.java` | `Call` | 函数调用 |
| `ir/inst/Move.java` | `Move` | 值移动 |
| `ir/inst/Phi.java` | `Phi` | SSA Phi 节点 |
| `ir/inst/Branch.java` | `Branch` | 无条件跳转 |
| `ir/inst/CondBranch.java` | `CondBranch` | 条件跳转 |
| `ir/inst/Return.java` | `Return` | 函数返回 |
| `ir/inst/GlobalAddr.java` | `GlobalAddr` | 全局变量地址 |
| `ir/block/BasicBlock.java` | `BasicBlock` | 基本块 |
| `ir/block/Function.java` | `Function` | 函数 |
| `ir/block/Module.java` | `Module` | 模块（编译单元） |
| `common/Type.java` | `Type` | 类型枚举（INT/VOID） |

### 优化 pass（接口依赖参考）

| 文件路径 | 类 | 依赖的 IR 接口 |
|---|---|---|
| `opt/ControlFlowGraph.java` | `ControlFlowGraph` | `Block.terminator()`, `Branch.target()`, `CondBranch.trueTarget/falseTarget()`, `Label → Block` 解析 |
| `opt/DominatorTree.java` | `DominatorTree` | `Function.blocks()`, `CFG.predecessors()`, Block identity |
| `opt/Mem2Reg.java` | `Mem2Reg` | `Alloca.result()`, `Load.address()`, `Store.address()`, `Store.value()`, `Phi.addIncoming()`, `Function.locals()`, `ControlFlowGraph` |
| `opt/PhiLowerer.java` | `PhiLowerer` | `Phi.incoming()`, `BasicBlock.phis()`, `BasicBlock.instructions()` |

---

## Java IR 类型映射

### 1. Type 枚举

```java
// Java — toyc.common.Type
public enum Type { INT, VOID }
```

```cpp
// C++ — toyc/ir/type.h
enum class Type { Int, Void };
```

### 2. 值身份（Value Identity）

**Java 规则：**
- 不重写 `equals()`/`hashCode()` — 全部使用对象引用身份（`Object` 默认）
- 优化 pass 中统一使用 `IdentityHashMap` / `Collections.newSetFromMap(new IdentityHashMap<>())`
- 两个 `Temp(0, INT)` 是不同的对象，不是同一个值

**C++ 策略：值指针身份（Value*）**
- 值通过 `new` 在 arena 中分配，通过裸指针 `Value*` 引用
- 每个值指针代表唯一身份
- 不允许值拷贝（禁用拷贝构造/赋值）
- 显示名称（`name()`）仅用于打印，不参与身份判定

### 3. 所有权模型

| Java | C++ |
|------|-----|
| GC 自动管理 | `FunctionContext` (内部类) 持有 `unique_ptr<Value>` |
| Module 通过 `List<GlobalVar>` 持有全局变量 | `Module` 通过 `vector<unique_ptr<Value>>` 持有全局变量和全局常量 |
| Function 通过 `List<LocalVar>` 持有局部变量 | `Function` 通过 `FunctionContext` 持有 `Temp`, `LocalVar`, `Label` |
| BasicBlock 通过 `ArrayList<Instruction>` 持有指令 | `BasicBlock` 通过 `vector<unique_ptr<Instr>>` 持有所有指令 |
| 指令通过强引用指向操作数值 | 指令通过 raw `Value*` 指回操作数 |
| 无反向指针（指令不知所属 block） | 可选：指令存储 `Block*` parent（默认不启用，基本块方法提供） |

**生命周期保证：**
- `Module` → `Function` → `BasicBlock` → `Instr`（unique_ptr 链）
- `Value` 的生命周期完全由所属层级控制
- Raw `Value*` 指针仅在同级或上级活跃时有效
- 在 pass 运行期间，所有指针有效

### 4. Value 子类型

| Java 类 | C++ 类 | 关键差异 |
|---------|--------|---------|
| `Temp(int index, Type type)` | `Temp(ValueId id, int index, Type type)` | 添加显式 ValueId |
| `LocalVar(String name, int index, boolean param)` | `LocalVar(ValueId id, String name, int index, bool param)` | 同上 |
| `GlobalVar(String name, int init)` | `GlobalVar(ValueId id, String name, int init)` | 同上 |
| `Constant.of(int value)` | `Constant::of(ValueId id, int value)` | Factory method |
| `Constant.named(String name, int value)` | `Constant::named(ValueId id, String name, int value)` | Factory method |
| `Label(String name)` | `Label(ValueId id, String name)` | 同上 |

### 5. 指令模型

采用**继承层次**（与 Java 一致），基类 `Instr` 提供公共接口：

```cpp
class Instr {
public:
    virtual ~Instr() = default;
    virtual Value* result() const { return nullptr; }
    virtual SmallVec<Value*> operands() const { return {}; }
    virtual bool is_terminator() const { return false; }
    virtual bool has_side_effect() const { return false; }
    virtual InstrKind kind() const = 0;
};
```

每个指令子类存储其专属数据字段。

### 6. 指令完整映射

| Java 类 | C++ 类 | result() | operands() | isTerminator | sideEffect |
|---------|--------|----------|------------|--------------|------------|
| `Alloca` | `AllocaInstr` | `LocalVar*` | empty | no | yes |
| `Load` | `LoadInstr` | `Temp*` | `[address]` | no | no |
| `Store` | `StoreInstr` | null | `[value, address]` | no | yes |
| `LoadImm` | `LoadImmInstr` | `Temp*` | `[constant]` | no | no |
| `BinaryOp` | `BinaryOpInstr` | `Temp*` | `[left, right]` | no | no |
| `UnaryOp` | `UnaryOpInstr` | `Temp*` | `[value]` | no | no |
| `Compare` | `CompareInstr` | `Temp*` | `[left, right]` | no | no |
| `Call` | `CallInstr` | `Temp*` (or null) | `[args...]` | no | yes |
| `Move` | `MoveInstr` | `Temp*` | `[value]` | no | no |
| `Phi` | `PhiInstr` | `Temp*` | `[values...]` | no | no |
| `Branch` | `BranchInstr` | null | empty | yes | no |
| `CondBranch` | `CondBranchInstr` | null | `[condition]` | yes | no |
| `Return` | `ReturnInstr` | null | `[value]` (optional) | yes | no |
| `GlobalAddr` | `GlobalAddrInstr` | `Temp*` | `[global]` | no | no |

### 7. BasicBlock

| Java | C++ |
|------|-----|
| `Label label` (final) | `Label* label_` (non-owning, owned by Function) |
| `List<Phi> phis` | `SmallVec<std::unique_ptr<Instr>> phis_` (Phi 子类的 unique_ptr 封装) |
| `List<Instruction> instructions` | `InstrList instrs_` (non-phi, non-terminator) |
| `Instruction terminator` | `std::unique_ptr<Instr> terminator_` |
| 无 parent 指针 | 可选 `Function* parent_` |
| `allInstructions()` | `all_instrs()` — 合并 phis + instrs + terminator |

### 8. CFG 设计

```cpp
class CFG {
    std::unordered_map<BlockId, BlockPtrVec> successors_;
    std::unordered_map<BlockId, BlockPtrVec> predecessors_;
    std::unordered_set<BlockId> reachable_;

public:
    static CFG build(Function& fn);
    
    std::span<BasicBlock* const> successors(BasicBlock* bb) const;
    std::span<BasicBlock* const> predecessors(BasicBlock* bb) const;
    bool is_reachable(BasicBlock* bb) const;
    const auto& reachable_blocks() const { return reachable_; }
};
```

Java 使用 `Map<Label, BasicBlock>` 解析分支目标。C++ 使用 `BlockId` 或直接 `BasicBlock*` 指针。由于 BasicBlock 在编译单元内分配且生命周期可控，可以用 `BasicBlock*` 作为 CFG 键。

### 9. IR Printer

继承 Java 的 visitor 模式思路，提供文本输出：

```cpp
class IRPrinter {
    std::string print(const Module& mod);
    std::string print(const Function& fn);
    std::string print(const BasicBlock& bb);
    std::string print(const Instr& instr);
    std::string print(const Value& val);
};
```

输出格式简洁、语义等价、稳定（不依赖 unordered 容器顺序）。

---

## Mem2Reg 所需接口（后续阶段参考）

从 `Mem2Reg.java` 分析，后续需要：

1. **`Function::locals()`** → 返回所有 `LocalVar*` 的映射
2. **`Instr::operands()`** → 遍历每条指令的操作数
3. **`LoadInstr::address()`** / **`StoreInstr::address()`** → 判断内存操作目标
4. **`PhiInstr::add_incoming(Label*, Value*)`** → 插入 Phi 前驱值
5. **`CFG::successors(block)`** → 遍历后继填充 Phi incoming
6. **`DominatorTree::dominance_frontier(block)`** → Phi 放置点
7. **`DominatorTree::children(block)`** → SSA renaming 递归
8. **`BasicBlock::replace_body(...)`** → 替换指令序列
9. **`BasicBlock::phis()`** → Phi 列表访问

---

## 序列化格式

```
func @main() -> i32 {
entry:
  %x.0 = alloca
  store i32 3, %x.0
  %t0 = load i32, %x.0
  ret i32 %t0
}
```

格式要求：
- `func @name(...)` 开始函数定义
- `label:` 开始基本块
- 每条指令一行，带缩进
- 值引用显示 DisplayName
- Phi: `%tN = phi [%val, %pred_label], [%val, %pred_label]`
- 稳定排序：按 block 顺序，block 内按 instr 顺序
