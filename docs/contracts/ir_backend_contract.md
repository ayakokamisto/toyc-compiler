# IR ↔ 后端接口契约

**维护者**：成员三（IR 生成方）& 成员四（代码生成方）  
**目标语言**：ToyC → RISC-V32 汇编  
**IR 格式**：**非 SSA**。vreg 是可多次写入的可变存储单元。  
**SSA 说明**：成员四可在后端内部将非 SSA IR 转换为 SSA 形式以支持优化，此过程对本契约完全透明，成员三无需感知。  
**约定原则**：成员三交付完整 `IRModule`；成员四仅依赖本文档，不感知 AST 任何细节。

> **变更规则**：任何字段/枚举/规则的修改必须双方确认后更新本文档，并在 Git commit message 中注明 `[contract]`。

---

## 1. IRModule 结构

`IRModule` 是成员三交付给成员四的唯一顶层对象。

```
IRModule
├── globalVars  : List<GlobalObject>     // 全局变量（仅 VAR，全局常量已内联，不在此列）
├── constTable  : ConstTable             // 所有常量的编译期值（仅供调试，后端不查询）
├── functions   : List<IRFunction>       // 所有函数定义（含 main）
└── funcTable   : Map<String, FuncMeta>  // 函数签名表，按函数名索引
```

```
IRFunction
├── name        : String
├── returnType  : Type             // INT | VOID
├── params      : List<Param>      // 按源码顺序，每个 Param 含 sourceName 和 vreg
├── basicBlocks : List<BasicBlock> // 第 0 个为入口块（label 固定为 "entry"）
└── symTable    : SymbolTable      // 该函数作用域内的局部符号表
```

```
Param
├── sourceName : String   // 源码中的形参名（仅供调试）
└── vreg       : String   // 分配给该形参的唯一 vreg，如 %p0、%p1
```

```
FuncMeta
├── name       : String
├── returnType : Type
└── paramTypes : List<Type>
```

**关于 `-opt` 参数**：成员三交付的 `IRModule` 结构与是否开启优化无关；`-opt` 标志由成员四读取，决定后端是否启用优化 Pass，对本契约透明。

---

## 2. SymbolTable 字段

每个 `IRFunction` 持有一个局部符号表。全局变量由 `IRModule.globalVars` 表示，全局常量已内联，两者均**不进**此符号表。

```
SymbolTable
└── entries : Map<VReg, SymbolEntry>
              // key = vreg 字符串（如 "%x_0"、"%p0"），全函数内唯一

SymbolEntry
├── vreg       : String           // 与 key 相同
├── sourceName : String           // 源码标识符（仅供调试）
├── kind       : LOCAL_VAR | PARAM
├── type       : INT              // ToyC 当前只有 int
└── scopeDepth : Int              // 0 = 函数顶层，1 = 第一层嵌套块，以此类推（仅供调试）
```

**规则：**
- **key 为 vreg，不是 source name**。同名变量在不同嵌套作用域中由成员三分配不同 vreg（如 `%x_0`、`%x_1`），后端无需关心作用域遮蔽。
- 局部常量（`const int`）**不进**此表。其值由成员三在 IR 构建时以 `CONST` 指令内联，后端不需要查询。
- 形参既进 symTable（kind=PARAM），也作为函数 `params` 列表的元素。

---

## 3. ConstTable 字段

存放所有在编译期已确定值的常量，**仅供调试参考，成员四在代码生成时不查询此表**。

```
ConstTable
└── entries : Map<String, Int32>
              // key 格式：
              //   全局常量 → "@name"
              //   局部常量 → "funcName#constId"（constId 由成员三保证唯一）
              // value = 编译期计算结果
```

**规则：**
- 成员三在构建 IR 时已将所有常量引用（全局/局部）替换为 `CONST <val>` 指令。
- 成员四遇到 `CONST` 指令时直接使用立即数，无需回查 ConstTable。
- 局部常量不分配运行期 vreg；ConstTable 的局部 key 仅用于调试定位，后端不依赖其具体格式。

---

## 4. GlobalObject 表示

**本表只包含全局变量（VAR）。全局常量已在 IR 中以 `CONST` 内联，不出现在此列表中。**

```
GlobalObject
├── name      : String    // 源码标识符，同时作为汇编标签（需为合法 C 标识符）
└── initValue : Int32     // 静态初始值（编译期已求值）
```

**汇编映射：**

| 类型 | 汇编段 | 示例指令 |
|------|--------|---------|
| 全局变量（VAR） | `.data` | `.word <initValue>` |

**访问方式（成员三保证）：**
- 全局变量读取：`LOAD_GLOBAL %dst, @name`
- 全局变量写入：`STORE_GLOBAL @name, %src`
- 全局变量的 `@name` 必须出现在 `globalVars` 列表中
- **全局常量绝不会产生 `LOAD_GLOBAL` 或 `STORE_GLOBAL` 指令**
- 成员二/三保证所有 `GlobalObject.initValue` 都已在编译期求值；后端不支持动态全局初始化。

---

## 5. IRInstruction 枚举

**非 SSA 局部变量模型：**  
所有 vreg 均为**可变存储单元**，可被 `COPY` / `CONST` 多次写入。后端按照"每个 vreg 对应一个栈槽或物理寄存器"处理，无需区分"单定义临时值"和"多定义变量"。

**vreg 分配来源：**  
后端必须为 IR 指令中出现的**所有** vreg 分配存储位置，包括 `%t0` 这类临时结果。`symTable` 只记录源码变量和形参的调试/作用域信息，不是后端分配栈槽或寄存器的完整来源。

### 5.1 算术指令

```
ADD   %dst, %src1, %src2     // dst = src1 + src2
SUB   %dst, %src1, %src2     // dst = src1 - src2
MUL   %dst, %src1, %src2     // dst = src1 * src2
DIV   %dst, %src1, %src2     // dst = src1 / src2（有符号整除）
MOD   %dst, %src1, %src2     // dst = src1 % src2（有符号取模）
NEG   %dst, %src             // dst = -src（一元负号）
```

### 5.2 比较指令（结果为 0 或 1）

```
EQ    %dst, %src1, %src2     // dst = (src1 == src2) ? 1 : 0
NE    %dst, %src1, %src2     // dst = (src1 != src2) ? 1 : 0
LT    %dst, %src1, %src2     // dst = (src1 <  src2) ? 1 : 0
LE    %dst, %src1, %src2     // dst = (src1 <= src2) ? 1 : 0
GT    %dst, %src1, %src2     // dst = (src1 >  src2) ? 1 : 0
GE    %dst, %src1, %src2     // dst = (src1 >= src2) ? 1 : 0
```

### 5.3 逻辑指令

```
LNOT  %dst, %src             // dst = (!src) ? 1 : 0（逻辑非）
```

> `&&` 和 `||` **没有**对应指令，必须在 IR 生成阶段降级为 `BRANCH` 序列（见第 8 节）。

### 5.4 数据移动指令

```
CONST %dst, <imm>            // dst = 立即数（Int32，可多次对同一 %dst 写入）
COPY  %dst, %src             // dst = src（可多次对同一 %dst 写入，非 SSA 合法）
```

### 5.5 全局变量访存

```
LOAD_GLOBAL   %dst, @name    // dst = 全局变量 name 的值
STORE_GLOBAL  @name, %src    // 全局变量 name = src
```

### 5.6 函数调用

```
CALL      %dst, <funcName>, [%arg0, %arg1, ...]   // int 返回值函数
CALL_VOID <funcName>, [%arg0, %arg1, ...]          // void 函数
```

### 5.7 控制流（终结指令，每个 BasicBlock 恰好一条）

```
JUMP    <label>                              // 无条件跳转
BRANCH  %cond, <true_label>, <false_label>  // 条件跳转（cond 非零为真）
RETURN  %src                                // 带值返回（int 函数）
RETURN                                      // 无值返回（void 函数）
```

---

## 6. BasicBlock / CFG 规则

```
BasicBlock
├── label        : String          // 全函数内唯一（命名规范见下）
├── instructions : List<IRInstr>   // 非终结指令序列
└── terminator   : IRInstr         // 恰好一条：JUMP | BRANCH | RETURN
```

**成员三保证的不变式（成员四可依赖）：**

1. 每个 `BasicBlock` 的 `terminator` 必须是 `JUMP`、`BRANCH` 或 `RETURN` 之一。
2. 函数第一个 BasicBlock 的 label 固定为 `"entry"`。
3. `BRANCH` 的两个目标 label 均为同函数内已存在的 BasicBlock label。
4. CFG 中不存在无法从 `entry` 到达的孤岛块（成员三负责死块消除）。
5. **（非 SSA）同一 vreg 可在多个块或同一块内被多次写入**，后端按可变存储单元统一处理。

**BasicBlock label 命名规范：**

- 字符集：`[A-Za-z0-9_]`（不含点号），以字母或下划线开头。
- 格式：`<语义>_<编号>`，如 `if_then_0`、`while_cond_1`、`loop_exit_2`。
- 成员四在发射汇编时，在非入口 label 前添加函数名前缀以避免跨函数冲突：`funcName__label`，例如 `main__if_then_0`。
- `entry` 块映射为函数符号本身（如 `main:`）。成员三保证不会生成跳转到 `entry` 的控制流边。

---

## 7. 函数调用规则

### 7.1 参数传递约定（IR 层）

- 实参按从左到右顺序填入 `CALL` 的参数列表。
- 成员三保证实参数量与 `FuncMeta.paramTypes` 一致。

### 7.2 参数到寄存器/栈的映射（后端实现，成员四负责）

| 参数位置 | 传递方式 |
|----------|---------|
| 第 1–8 个参数 | → `a0`–`a7` |
| 第 9 个及以后 | 由**调用者**在调用前写入调用栈参数区；调用返回后由调用者回收参数区 |

**栈上传参精确布局：**
- 调用者在 `call` 前一次性预留 `stackArgSize` 字节，大小覆盖第 9 个及以后的参数，并按 16 字节对齐。
- 第 9 个参数写入 `0(sp)`，第 10 个参数写入 `4(sp)`，之后每个参数偏移递增 4 字节。
- 被调用者建立帧指针后，第 9 个参数位于 `0(s0)`，第 10 个参数位于 `4(s0)`，以此类推。
- 调用返回后，调用者执行 `addi sp, sp, stackArgSize` 回收调用栈参数区。

**函数入口参数落位：**
- 函数入口时，成员四必须将所有 `Param.vreg` 从 ABI 传参位置初始化到后端管理的 vreg 存储位置。
- 初始化后，`Param.vreg` 与普通 vreg 一样处理，不再假设其永久绑定在 `a0`–`a7`。

### 7.3 返回值约定（IR 层）

- `int` 函数：`CALL %dst, ...`，成员四将返回值从 `a0` 取出放入 `%dst` 对应位置。
- `void` 函数：`CALL_VOID ...`，成员四**不得**读取 `a0` 作为结果。

### 7.4 调用者/被调用者保存（后端责任）

| 类别 | 寄存器 | 保存方 |
|------|--------|--------|
| 返回地址 | `ra` | 被调用者（每个函数序言保存） |
| 帧指针 | `s0` | 被调用者 |
| 参数/返回值 | `a0`–`a7` | 调用者保存（如调用后还需用） |
| 临时 | `t0`–`t6` | 调用者保存 |
| 保留 | `s1`–`s11` | 被调用者（**使用了哪些就保存哪些**） |

### 7.5 递归调用

ToyC 支持函数递归。成员三不做特殊处理；成员四必须在每个函数序言保存 `ra`，否则递归调用会覆盖返回地址。

---

## 8. 短路逻辑降级规则

`&&` 和 `||` 在 IR 中**必须**展开为 `BRANCH` 序列。

**非 SSA 说明**：`%result_N` 在 `true` 和 `false` 两个块中各被 `CONST` 写入一次，共两次写入。这在非 SSA 语义下完全合法——两条路径运行时只有一条执行，后端将 `%result_N` 分配为单一可变存储单元即可。

### 8.1 逻辑与（`A && B`）

```
<eval A into %a>
BRANCH %a, and_rhs_N, and_false_N

and_rhs_N:
<eval B into %b>
BRANCH %b, and_true_N, and_false_N

and_true_N:
CONST %result_N, 1          // 第一次写入 %result_N
JUMP  and_end_N

and_false_N:
CONST %result_N, 0          // 第二次写入 %result_N（非 SSA 合法）
JUMP  and_end_N

and_end_N:
// %result_N 即为 (A && B) 的值（0 或 1）
```

### 8.2 逻辑或（`A || B`）

```
<eval A into %a>
BRANCH %a, or_true_N, or_rhs_N

or_rhs_N:
<eval B into %b>
BRANCH %b, or_true_N, or_false_N

or_true_N:
CONST %result_N, 1
JUMP  or_end_N

or_false_N:
CONST %result_N, 0
JUMP  or_end_N

or_end_N:
// %result_N 即为 (A || B) 的值（0 或 1）
```

### 8.3 用作 if/while 条件时的简化（可选）

当 `&&`/`||` 直接作为 `if`/`while` 的条件时，成员三可省略 `%result_N` 的写回，将 `and_true_N`/`and_false_N` 直接替换为 `if_then`/`if_else` 标签，避免产生冗余 CONST+COPY。

---

## 9. 后端不负责的语义检查

以下检查由**前端成员协作完成**（具体由成员二语义分析或成员三构建 IR 时保证），成员四收到 `IRModule` 时视为已保证，**不做二次校验**：

| 序号 | 保证内容 |
|------|---------|
| 1 | 所有变量在使用前已声明 |
| 2 | 常量不出现在赋值左侧（无 `STORE_GLOBAL @const_name` 指令，无 `COPY %const_vreg` 指令） |
| 3 | `void` 函数的 `RETURN` 指令无操作数 |
| 4 | `int` 函数每条路径均以带操作数的 `RETURN` 结尾 |
| 5 | `break`/`continue` 只出现在循环对应的 CFG 结构中 |
| 6 | 函数调用参数数量与签名匹配 |
| 7 | `void` 函数调用结果不被用作右值（不产生 `CALL %dst ...` 形式） |
| 8 | 所有常量初始值已在编译期求值为 `Int32` |
| 9 | 全局变量的初始化表达式已在编译期求值（`GlobalObject.initValue` 已填入；不会传入动态全局初始化） |
| 10 | 函数调用发生在被调函数声明之后（无前向引用） |

---

## 10. 示例：ToyC 源码 → IR → RISC-V32 汇编

### 10.1 源码

```c
const int LIMIT = 10;   // 全局常量，IR 中内联为 CONST 10，不进 globalVars

int add(int a, int b) {
    return a + b;
}

int main() {
    int x = 3;
    int y = 4;
    int z = 0;          // 非 SSA：%z 会被多次写入
    if (x < LIMIT) {
        z = add(x, y);
    }
    return z;
}
```

### 10.2 IRModule（文本表示）

```
IRModule {
  globalVars  = []            // LIMIT 是常量，不在此列
  constTable  = { "@LIMIT" → 10 }   // 仅供调试

  funcTable = {
    "add"  → FuncMeta { returnType=INT,  paramTypes=[INT, INT] }
    "main" → FuncMeta { returnType=INT,  paramTypes=[] }
  }

  functions = [

    // ── function: add ──────────────────────────────────────────
    IRFunction {
      name = "add", returnType = INT
      params = [ {sourceName="a", vreg="%p0"}, {sourceName="b", vreg="%p1"} ]
      symTable = {
        "%p0" → { sourceName="a", kind=PARAM,     type=INT, scopeDepth=0 }
        "%p1" → { sourceName="b", kind=PARAM,     type=INT, scopeDepth=0 }
      }
      basicBlocks = [
        BasicBlock {
          label = "entry"
          instructions = [
            ADD %t0, %p0, %p1
          ]
          terminator = RETURN %t0
        }
      ]
    }

    // ── function: main ─────────────────────────────────────────
    IRFunction {
      name = "main", returnType = INT
      params = []
      symTable = {
        "%x" → { sourceName="x", kind=LOCAL_VAR, type=INT, scopeDepth=0 }
        "%y" → { sourceName="y", kind=LOCAL_VAR, type=INT, scopeDepth=0 }
        "%z" → { sourceName="z", kind=LOCAL_VAR, type=INT, scopeDepth=0 }
      }
      basicBlocks = [

        BasicBlock {
          label = "entry"
          instructions = [
            CONST %x,  3
            CONST %y,  4
            CONST %z,  0           // %z 第一次写入
            CONST %t1, 10          // LIMIT 内联为立即数
            LT    %t2, %x, %t1
          ]
          terminator = BRANCH %t2, if_then_0, if_end_0
        }

        BasicBlock {
          label = "if_then_0"
          instructions = [
            CALL  %t3, "add", [%x, %y]
            COPY  %z, %t3          // %z 第二次写入（非 SSA 合法）
          ]
          terminator = JUMP if_end_0
        }

        BasicBlock {
          label = "if_end_0"
          instructions = []
          terminator = RETURN %z   // 读取 %z 当前值
        }

      ]
    }
  ]
}
```

### 10.3 RISC-V32 汇编（成员四产出）

```asm
    .data
    # （无全局变量）

    .text

    # ── function: add ──────────────────────────────────────────
    .global add
add:
    addi sp, sp, -16
    sw   ra,  12(sp)        # 保存返回地址
    sw   s0,   8(sp)        # 保存帧指针
    addi s0,  sp, 16        # 建立帧指针

    # 示例中 %p0/%p1 在叶子函数内直接使用 a0/a1；通用实现仍需按第 7.2 节完成参数落位
    add  a0, a0, a1          # %t0 = ADD %p0, %p1；结果直接放入 a0

    lw   ra,  12(sp)
    lw   s0,   8(sp)
    addi sp,  sp, 16
    ret                      # RETURN %t0

    # ── function: main ─────────────────────────────────────────
    .global main
main:
    addi sp, sp, -32
    sw   ra,  28(sp)        # 保存返回地址
    sw   s0,  24(sp)        # 保存帧指针
    sw   s1,  20(sp)        # 保存 s1（callee-saved，下面会用到）
    sw   s2,  16(sp)        # 保存 s2
    sw   s3,  12(sp)        # 保存 s3
    addi s0,  sp, 32        # 建立帧指针

    li   s1, 3               # %x = CONST 3
    li   s2, 4               # %y = CONST 4
    li   s3, 0               # %z = CONST 0（第一次写入）

    li   t0, 10              # LIMIT 内联（CONST 10）
    slt  t1, s1, t0          # %t2 = LT %x, %t1
    beqz t1, main__if_end_0  # BRANCH %t2, if_then_0, if_end_0

main__if_then_0:
    mv   a0, s1              # 第 1 个参数 x → a0
    mv   a1, s2              # 第 2 个参数 y → a1
    call add                 # CALL "add", [%x, %y]
    mv   s3, a0              # COPY %z, %t3（%z 第二次写入）

main__if_end_0:
    mv   a0, s3              # RETURN %z

    lw   ra,  28(sp)        # 恢复返回地址
    lw   s0,  24(sp)        # 恢复帧指针
    lw   s1,  20(sp)        # 恢复 s1
    lw   s2,  16(sp)        # 恢复 s2
    lw   s3,  12(sp)        # 恢复 s3
    addi sp,  sp, 32
    ret
```

---

## 附录：虚拟寄存器命名约定

| 前缀 | 含义 | 示例 |
|------|------|------|
| `%p` | 函数形参 | `%p0`, `%p1` |
| `%t` | 临时计算结果（表达式中间值） | `%t0`, `%t1` |
| `%` + 唯一局部名 | 局部变量（可带编号区分遮蔽；无冲突时可省略编号） | `%x`, `%x_0`, `%x_1` |
| `@` + 名称 | 全局变量 | `@g_count` |

**唯一性保证**：同一函数内所有 vreg 字符串全局唯一（由成员三在 IR 生成时用计数器保证）。  
**汇编标签**：`entry` 映射为函数符号本身；其他 IR label 在发射时加函数名前缀（`funcName__label`）以避免跨函数冲突。
