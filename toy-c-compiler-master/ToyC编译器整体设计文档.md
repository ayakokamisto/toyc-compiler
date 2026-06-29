# ToyC 编译器整体设计文档

## 一、项目概述

ToyC 编译器将 ToyC 语言（C 语言的简化子集）编译为可正确执行的 RISC-V 32 位汇编代码。本项目采用 **Java（JDK 21）** 开发，**Maven** 构建，**ANTLR 4.13.1** 作为前端解析工具。编译器独立完成全部核心编译阶段，不依赖 LLVM、Clang 等现成后端框架。

**编译流水线**：源程序（stdin）→ 词法分析 → 语法分析 → AST 构建 → 语义分析 → IR 生成 → 代码优化 → RISC-V 汇编（stdout）

**入口与参数**：

- 从标准输入读取 ToyC 源程序，向标准输出写入汇编代码
- 支持 `-opt` 参数启用优化

---

## 二、总体架构

编译器划分为前端（Frontend）、中间表示与优化（IR & Opt）、后端（Backend）三部分，包含六个核心阶段：

| 阶段 | 职责 | 工具/模块 |
|------|------|-----------|
| 词法分析 | 将字符流转换为 Token 流 | ANTLR4 Lexer |
| 语法分析 | 将 Token 流构建为解析树 | ANTLR4 Parser |
| AST 构建 | 解析树 → 强类型抽象语法树 | `ParserFacade` / `ASTBuilder` |
| 语义分析 | 符号表构建、类型检查、作用域分析 | `SemanticAnalyzer` |
| IR 生成与优化 | AST → 三地址码 → 平台无关优化 | `IRBuilder` / `Optimizer` |
| 目标代码生成 | IR → RISC-V 32 汇编 | `RiscVEmitter` |

---

## 三、前端设计

> 词法分析与语法分析均通过 ANTLR4 框架实现。词法规则和语法规则统一写在 `ToyC.g4` 中，ANTLR 自动生成 `ToyCLexer` 和 `ToyCParser`，无需手写词法/语法分析器。

### 3.1 词法分析

词法规则与语法规则统一维护在 `src/main/antlr/ToyC.g4` 中。Maven 构建时通过 `antlr4-maven-plugin` 生成 `ToyCLexer`，运行时由 `ParserFacade` 使用 `CharStreams.fromString(source)` 将标准输入中的源程序转为 ANTLR 字符流，再交给 `ToyCLexer` 扫描得到 Token 序列。词法阶段不手写状态机，而是利用 ANTLR 根据规则自动构造等价的词法自动机，保证规则集中、行为可复现。

#### 关键 Token 定义

| Token | 说明 | 规则 |
|-------|------|------|
| `ID` | 标识符 | `[_A-Za-z][_A-Za-z0-9]*` |
| `NUMBER` | 整数常量 | `'0' \| [1-9][0-9]*` |
| 关键字 | `CONST`, `INT`, `VOID`, `IF`, `ELSE`, `WHILE`, `BREAK`, `CONTINUE`, `RETURN` | 固定小写串 |
| 运算符 | `PLUS`, `MINUS`, `STAR`, `SLASH`, `PERCENT`, `LT`, `GT`, `LE`, `GE`, `EQ`, `NE`, `NOT`, `AND`, `OR`, `ASSIGN` | `+`, `-`, `*`, `/`, `%`, `<`, `>`, `<=`, `>=`, `==`, `!=`, `!`, `&&`, `\|\|`, `=` |
| 分隔符 | `LPAREN`, `RPAREN`, `LBRACE`, `RBRACE`, `COMMA`, `SEMI` | `(`, `)`, `{`, `}`, `,`, `;` |

#### 规则优先级与冲突处理

ANTLR 词法器遵循“最长匹配优先，长度相同按规则声明顺序优先”的原则，因此 `ToyC.g4` 中将关键字规则放在 `ID` 之前，将 `<=`、`>=`、`==`、`!=`、`&&`、`||` 等多字符运算符放在相关单字符运算符之前。这样可以保证：

- `int`、`while` 等完整关键字被识别为对应关键字 Token，而不是普通 `ID`。
- `integer`、`while1`、`const_value` 等只包含关键字前缀的词仍按最长匹配识别为 `ID`。
- `<=`、`==`、`&&` 等不会被拆成两个单字符 Token。
- 语言大小写敏感，`Main`、`INT`、`While` 均为普通标识符而非关键字。

#### 关于 NUMBER 的设计说明

任务需求文档中 `NUMBER` 正则表达式为 `-?(0|[1-9][0-9]*)`（包含可选前缀 `-`），但语法文件中 `NUMBER` 仅匹配非负整数，负号交由解析器的一元表达式处理。**原因**：若词法层将 `-` 纳入 `NUMBER`，`1-5`（无空格减法）将被错误 Tokenize 为 `NUMBER(1) NUMBER(-5)`，破坏减法表达式的解析。将 `-` 作为独立的 `MINUS` Token，在解析器层通过 `unaryExpr: MINUS unaryExpr` 处理取反操作，是 C 语言家族编译器的标准做法，语义上完全等价——`-42` 解析为 `Unary(MINUS, IntLiteral(42))`。

整数 Token 只接受十进制形式：`0` 或非零数字开头的数字串。不支持八进制、十六进制、浮点数、字符常量和字符串字面量，符合 ToyC 语言定义的精简范围。`NUMBER` 在 AST 构建阶段通过 `Integer.parseInt` 转换为 `Expr.IntLiteral`，后续溢出或非法数值会作为编译失败处理。

#### 空白字符与注释

空白字符和注释在词法阶段使用 `-> skip` 丢弃，不进入 `CommonTokenStream`，因此不会影响语法分析。ANTLR 在跳过换行时仍维护行号和列号，后续错误提示可以定位到源文件中的具体位置。

- 单行注释：`//` 至行尾
- 多行注释：`/* ... */`
- 空白字符：空格、制表符、回车、换行

多行注释采用非贪婪匹配 `.*?`，遇到最近的 `*/` 即结束，与任务要求中“以最近的 `*/` 结束”一致。ToyC 与 C 语言一样不支持嵌套块注释。

#### 词法错误处理与接口

`ParserFacade` 会移除 ANTLR 默认错误监听器，并为词法器和语法器统一注册 `ThrowingErrorListener`。当词法阶段遇到无法识别的字符、块注释未闭合等问题时，监听器立即抛出 `IllegalArgumentException`，错误信息格式为 `syntax error at 行号:列号: 具体原因`，由主入口 `Main` 捕获后输出到标准错误并终止编译。

词法阶段的输出是 `CommonTokenStream`，直接作为 `ToyCParser` 的输入。由于课程评测用例保证源程序无词法/语法错误，正常编译路径中 Token 流只承担前端结构化输入的职责；异常路径主要用于本地调试和报告说明。

### 3.2 语法分析

与词法规则同文件（`ToyC.g4`），ANTLR 自动生成 `ToyCParser`。文法与 ToyC 语言定义严格对应：

```
compUnit  →  (decl | funcDef)+ EOF
decl      →  constDecl | varDecl
funcDef   →  (INT|VOID) ID '(' (param (',' param)*)? ')' block
param     →  INT ID
block     →  '{' stmt* '}'
stmt      →  block | ';' | expr ';' | ID '=' expr ';' | decl
           |  'if' '(' expr ')' stmt ('else' stmt)?
           |  'while' '(' expr ')' stmt
           |  'break' ';' | 'continue' ';' | 'return' expr? ';'
expr      →  lOrExpr
lOrExpr   →  lAndExpr ('||' lAndExpr)*
lAndExpr  →  relExpr ('&&' relExpr)*
relExpr   →  addExpr (('<'|'>'|'<='|'>='|'=='|'!=') addExpr)*
addExpr   →  mulExpr (('+'|'-') mulExpr)*
mulExpr   →  unaryExpr (('*'|'/'|'%') unaryExpr)*
unaryExpr →  primaryExpr | ('+'|'-'|'!') unaryExpr
primary   →  ID | NUMBER | '(' expr ')' | ID '(' (expr (',' expr)*)? ')'
```

### 3.3 当前支持的 ToyC 语法

当前 ToyC 语言子集支持 `int` 与 `void` 函数、整型变量/常量、基础控制流、函数调用和整型表达式。所有可执行语义均围绕 32 位整数值展开。

#### 3.3.1 编译单元

一个 ToyC 程序由至少一个顶层声明或函数定义组成：

```c
int g = 5;
const int limit = 10;

int main() {
    return g + limit;
}
```

当前语义要求程序必须定义 `int main()`，且 `main` 不能带参数。

#### 3.3.2 类型与函数

当前支持的函数返回类型：

- `int`
- `void`

函数形参目前只支持 `int ID` 形式：

```c
int add(int a, int b) {
    return a + b;
}

void noop(int x) {
    return;
}
```

函数可以递归调用，也可以在函数体内调用其他已定义函数：

```c
int fib(int n) {
    if (n == 0) {
        return 0;
    }
    if (n == 1) {
        return 1;
    }
    return fib(n - 1) + fib(n - 2);
}
```

#### 3.3.3 变量与常量声明

支持全局和局部的 `int` 变量声明：

```c
int g = 1;

int main() {
    int x = 2;
    x = x + g;
    return x;
}
```

支持全局和局部的 `const int` 常量声明：

```c
const int base = 3;

int main() {
    const int local = base + 2;
    return local;
}
```

当前变量声明必须带初始化表达式，即支持 `int x = expr;`，不支持单独的 `int x;`。`const` 初始化表达式必须能在编译期求值。全局变量初始化表达式也必须能在编译期求值，便于后端生成 `.data` 段初始值。

#### 3.3.4 语句

当前支持以下语句：

| 语句 | 示例 |
|------|------|
| 语句块 | `{ stmt* }` |
| 空语句 | `;` |
| 表达式语句 | `foo(1);` |
| 赋值语句 | `x = x + 1;` |
| 局部声明语句 | `int x = 0;` / `const int c = 1;` |
| 条件语句 | `if (cond) stmt` / `if (cond) stmt else stmt` |
| 循环语句 | `while (cond) stmt` |
| 循环控制 | `break;` / `continue;` |
| 返回语句 | `return expr;` / `return;` |

示例：

```c
int main() {
    int i = 0;
    int sum = 0;
    while (i < 5) {
        i = i + 1;
        if (i == 3) {
            continue;
        }
        sum = sum + i;
    }
    return sum;
}
```

`break` 和 `continue` 只能出现在 `while` 循环内部。

#### 3.3.5 表达式与运算符

当前所有普通表达式结果均为 `int`。支持：

| 类别 | 运算符/形式 |
|------|-------------|
| 整数字面量 | `0`, `1`, `123` |
| 标识符引用 | `x`, `sum`, `g` |
| 函数调用 | `add(1, 2)` |
| 括号表达式 | `(a + b) * c` |
| 一元运算 | `+x`, `-x`, `!x` |
| 算术运算 | `+`, `-`, `*`, `/`, `%` |
| 关系/相等比较 | `<`, `>`, `<=`, `>=`, `==`, `!=` |
| 逻辑运算 | `&&`, `||` |

运算优先级从高到低为：

1. 函数调用、标识符、字面量、括号表达式
2. 一元 `+`、`-`、`!`
3. `*`、`/`、`%`
4. `+`、`-`
5. `<`、`>`、`<=`、`>=`、`==`、`!=`
6. `&&`
7. `||`

逻辑与 `&&`、逻辑或 `||` 在 IR 生成阶段保留短路语义。条件判断遵循 C 风格的非零为真、零为假。

#### 3.3.6 暂不支持的语法

当前语言子集暂不支持：

- 数组、指针、结构体、字符串和字符字面量。
- 浮点数和除 `int`、`void` 以外的类型。
- 变量无初始化声明，例如 `int x;`。
- 多变量同语句声明，例如 `int a = 1, b = 2;`。
- `for`、`do while`、`switch`、`goto`。
- 自增/自减、复合赋值，例如 `i++`、`x += 1`。
- 函数声明原型，函数必须以定义形式出现在源码中。
- `return` 以外的隐式返回值规则；`int` 函数必须保证所有可能路径返回整数值。

### 3.4 抽象语法树（AST）

AST 基于 Java 21 `sealed interface` 和 `record` 实现，节点**轻量且不可变**。通过访问者模式（Visitor Pattern）将数据结构与遍历算法解耦，后续语义分析、IR 生成等环节各自实现为独立的 Visitor。

#### 3.4.1 核心接口

```java
public interface ASTNode {
    <R, C> R accept(ASTVisitor<R, C> visitor, C context);
}
```

泛型参数 `R`（返回值类型）和 `C`（上下文类型）使 Visitor 可携带上下文并返回任意类型结果，天然适配语义分析中的属性传递。

节点分为三大类，均以 `sealed interface` 约束实现：

| 类型 | 接口 | 说明 |
|------|------|------|
| 声明 | `Decl extends Stmt` | 变量/常量声明，可作为语句出现 |
| 语句 | `Stmt` | 各类控制流和基本语句 |
| 表达式 | `Expr` | 计算求值的表达式 |

#### 3.4.2 编译单元与顶层节点

| 节点 | 签名 | 说明 |
|------|------|------|
| `Program` | `record Program(List<ASTNode> defs)` | 语法树根节点，包含全局声明与函数定义 |
| `FuncDef` | `record FuncDef(Type returnType, String id, List<Param> params, Stmt.Block body)` | 函数定义，`Type` 为 `INT \| VOID` |
| `FuncDef.Param` | `record Param(String id)` | 形参（隐式 `int` 类型） |

#### 3.4.3 声明

| 节点 | 签名 | 对应文法 |
|------|------|----------|
| `Decl.VarDecl` | `record VarDecl(String id, Expr init)` | `int ID = Expr;`，强制初始化 |
| `Decl.ConstDecl` | `record ConstDecl(String id, Expr init)` | `const int ID = Expr;`，需编译期求值 |

#### 3.4.4 语句

| 节点 | 签名 | 说明 |
|------|------|------|
| `Stmt.Block` | `record Block(List<Stmt> stmts)` | 语句块，创建嵌套作用域 |
| `Stmt.Empty` | `record Empty()` | 空语句 `;` |
| `Stmt.ExprStmt` | `record ExprStmt(Expr expr)` | 表达式语句 |
| `Stmt.Assign` | `record Assign(String id, Expr expr)` | 赋值 `ID = Expr;` |
| `Stmt.If` | `record If(Expr cond, Stmt thenStmt, Stmt elseStmt)` | 条件分支，`elseStmt` 可为 `null` |
| `Stmt.While` | `record While(Expr cond, Stmt body)` | while 循环 |
| `Stmt.Break` | `record Break()` | 跳出循环 |
| `Stmt.Continue` | `record Continue()` | 继续循环 |
| `Stmt.Return` | `record Return(Expr expr)` | 返回语句，`expr` 为 `null` 时用于 `void` 函数 |

#### 3.4.5 表达式

| 节点 | 签名 | 说明 |
|------|------|------|
| `Expr.Binary` | `record Binary(Expr left, BinaryOp op, Expr right)` | 二元运算 |
| `Expr.Unary` | `record Unary(UnaryOp op, Expr expr)` | 一元运算 |
| `Expr.Id` | `record Id(String name)` | 标识符引用 |
| `Expr.IntLiteral` | `record IntLiteral(int value)` | 整数常量 |
| `Expr.FuncCall` | `record FuncCall(String id, List<Expr> args)` | 函数调用 |

**运算符枚举**：

| 枚举 | 值 |
|------|-----|
| `BinaryOp` | `ADD`, `SUB`, `MUL`, `DIV`, `MOD`（算术）；`LT`, `GT`, `LE`, `GE`, `EQ`, `NEQ`（关系）；`AND`, `OR`（逻辑） |
| `UnaryOp` | `PLUS`, `MINUS`, `NOT` |

#### 3.4.6 访问者模式

```java
public interface ASTVisitor<R, C> {
    R visitProgram(Program node, C context);
    R visitFuncDef(FuncDef node, C context);
    R visitFuncDefParam(FuncDef.Param node, C context);
    R visitVarDecl(Decl.VarDecl node, C context);
    R visitConstDecl(Decl.ConstDecl node, C context);
    R visitBlockStmt(Stmt.Block node, C context);
    R visitEmptyStmt(Stmt.Empty node, C context);
    R visitExprStmt(Stmt.ExprStmt node, C context);
    R visitAssignStmt(Stmt.Assign node, C context);
    R visitIfStmt(Stmt.If node, C context);
    R visitWhileStmt(Stmt.While node, C context);
    R visitBreakStmt(Stmt.Break node, C context);
    R visitContinueStmt(Stmt.Continue node, C context);
    R visitReturnStmt(Stmt.Return node, C context);
    R visitBinaryExpr(Expr.Binary node, C context);
    R visitUnaryExpr(Expr.Unary node, C context);
    R visitIdExpr(Expr.Id node, C context);
    R visitIntLiteralExpr(Expr.IntLiteral node, C context);
    R visitFuncCallExpr(Expr.FuncCall node, C context);
}
```

每个具体 Record 通过 `accept()` 分发到对应的 `visit*` 方法，实现双重分派（Double Dispatch）。后续语义分析、IR 生成等 Pass 各自实现 `ASTVisitor` 接口即可。

### 3.5 解析门面（ParserFacade）

`ParserFacade` 封装 ANTLR 调用流程：

1. 创建 `ToyCLexer`，设置错误监听器（遇到错误直接抛异常）
2. 创建 `CommonTokenStream` 和 `ToyCParser`
3. 调用 `parser.compUnit()` 进行语法分析
4. 调用 `ASTBuilder` 将 ANTLR 解析树（`CompUnitContext`）转换为 AST `Program` 节点

> **当前状态**：词法、语法分析与"解析树 → AST"转换均已接入主流程，`ASTBuilder` 负责将 ANTLR 解析树转换为强类型 AST。

---

## 四、语义分析

语义分析阶段由 `SemanticAnalyzer` 实现，输入为 AST `Program`，输出为 `SemanticResult`。该阶段不修改 AST，也不生成 IR 或目标代码，主要负责验证程序是否满足 ToyC 语义约束，并为后续 IR/后端提供可直接使用的符号绑定和常量信息。

### 4.1 语义分析入口

主流程在 AST 构建完成后调用：

```java
SemanticResult sem = SemanticAnalyzer.analyze(ast);
```

若发现语义错误，分析器抛出 `SemanticException`，由主入口统一捕获并终止编译。当前语义错误信息以 `semantic error: ...` 为前缀，便于与词法/语法错误区分。

### 4.2 符号与作用域

语义阶段使用 `Scope` 表示嵌套作用域。每个作用域保存本层声明的变量、常量和函数符号，并通过 `parent` 指针向外层作用域查找。

| 符号类型 | 枚举值 | 说明 |
|----------|--------|------|
| 全局变量 | `GLOBAL_VAR` | 全局 `int` 变量 |
| 全局常量 | `GLOBAL_CONST` | 全局 `const int` 常量，保存编译期值 |
| 局部变量 | `LOCAL_VAR` | 函数体或块内 `int` 变量 |
| 局部常量 | `LOCAL_CONST` | 函数体或块内 `const int` 常量，保存编译期值 |
| 形参 | `PARAM` | 函数形参，记录参数序号 |

函数符号由 `FunctionSymbol` 单独表示，包含函数名、返回类型、形参符号列表和对应的 `FuncDef` 节点。函数只能定义在全局作用域中，且与全局变量/常量共享同一命名空间，不能重复定义。

作用域规则如下：

- 全局声明和函数定义进入全局作用域。
- 每个函数建立函数作用域，形参在函数作用域内可见。
- 每个 `Stmt.Block` 建立新的块级作用域，允许屏蔽外层变量、常量或形参。
- 变量、常量和函数调用均按声明顺序分析，因此使用必须发生在声明之后。
- 同一作用域内不允许重复声明同名变量、常量或函数。

### 4.3 语义检查规则

`SemanticAnalyzer` 基于 AST Visitor 遍历整棵语法树，当前实现覆盖以下 ToyC 语义约束：

| 类别 | 检查内容 |
|------|----------|
| 程序入口 | 程序必须包含 `int main()`，且 `main` 无参数 |
| 函数定义 | 函数只能位于全局作用域，函数名唯一 |
| 函数调用 | 只能调用已定义且未被局部符号屏蔽的函数；允许函数递归调用自身；实参数量必须与形参数量一致 |
| 标识符使用 | 变量、常量、形参必须先声明后使用；函数不能作为普通值使用 |
| 赋值语句 | 赋值左值必须是已声明的非常量符号，`const` 不能被修改 |
| 表达式类型 | ToyC 中普通表达式类型为 `int`；`void` 函数调用不能作为条件、赋值右值、返回值或运算操作数 |
| 控制流 | `break` 和 `continue` 只能出现在 `while` 循环内部 |
| 返回语句 | `int` 函数必须在所有可能执行路径上返回 `int` 值；`void` 函数不能返回表达式 |
| 全局变量 | 全局变量初始化表达式必须能在编译期求值，便于后端生成静态数据段 |
| 常量声明 | `const` 初始化表达式必须能在编译期求值 |

返回路径分析采用保守策略：

- `return` 语句视为必定返回。
- 顺序语句块中，只要某条语句之后必定返回，则该块必定返回。
- `if-else` 只有在 then 分支和 else 分支都必定返回时，整体才视为必定返回。
- `while` 即使条件为常量真，也暂不视为必定返回，以避免过度推断。

### 4.4 编译期常量求值

常量求值结果记录在 `SemanticResult.constValueOf` 中。当前支持以下表达式的编译期求值：

- 整数字面量。
- 已声明的 `const int` 标识符。
- 一元运算：`+`、`-`、`!`。
- 二元算术运算：`+`、`-`、`*`、`/`、`%`。
- 关系运算：`<`、`>`、`<=`、`>=`、`==`、`!=`。
- 逻辑运算：`&&`、`||`。

若 `const` 初始化表达式包含变量、函数调用或无法确定的表达式，则语义分析报错。对除零、取模零等情况，常量求值不会产生合法编译期值，因此也不能作为 `const` 初始化结果。

### 4.5 SemanticResult 输出信息

`SemanticResult` 是语义阶段提供给后续 IR/后端的统一结果对象，包含以下信息：

| 信息 | 接口 | 用途 |
|------|------|------|
| 全局作用域 | `globals()` | 查询全局变量、常量和函数符号 |
| 节点作用域 | `scopeOf(ASTNode)` | 获取 AST 节点对应的作用域 |
| 表达式类型 | `typeOf(Expr)` | 判断表达式结果类型，区分 `int` 与 `void` |
| 常量值 | `constValueOf(Expr)` | 获取可编译期确定的表达式值 |
| 声明绑定 | `symbolOf(Decl)` | 将变量/常量声明映射到对应 `Symbol` |
| 形参绑定 | `symbolOf(FuncDef.Param)` | 将形参节点映射到对应 `Symbol` |
| 赋值绑定 | `symbolOf(Stmt.Assign)` | 将赋值左值映射到被赋值的 `Symbol` |
| 标识符引用绑定 | `symbolOf(Expr.Id)` | 将变量/常量引用映射到声明处的 `Symbol` |
| 函数调用绑定 | `functionOf(Expr.FuncCall)` | 将函数调用映射到对应 `FunctionSymbol` |

通过这些绑定关系，后端无需重新按名字查找符号，可以直接依据 AST 节点查询语义结果，降低后续阶段重复实现符号解析逻辑的风险。

> **当前状态**：语义分析模块已接入主编译流程，并通过 `mvn test` 构建验证。该阶段仅负责语义检查和语义信息收集，不承担 IR 构建、优化或 RISC-V 汇编生成。

---

## 五、中间表示与代码优化

### 5.1 IR 设计目标

当前 IR 采用三地址码风格，并以 CFG（Control Flow Graph，控制流图）为核心组织函数内部结构。每个函数由若干基本块组成，基本块内部显式区分 `Phi`、普通指令和终结指令，便于后续进行控制流分析、SSA 构造、局部值优化以及后端代码生成。

IR 设计遵循两个原则：

1. **先表达清楚语义**：IRBuilder 可以生成较朴素的 `Alloca`、`Load`、`Store` 形式，保证从 AST 到 IR 的转换简单稳定。
2. **再由优化管线提升质量**：`-opt` 下通过 CFG 简化、Mem2Reg、Phi 降级、常量折叠、DSE 等 pass 清理冗余 IR，为后续寄存器分配和更强 SSA 优化留下空间。

### 5.2 Value 层

IR 值统一继承 `IRValue`，携带 `Type` 和 IR 名称：

| 类 | 说明 |
|----|------|
| `Constant` | 整数立即数或具名全局常量 |
| `GlobalVar` | 全局变量，记录初始值 |
| `LocalVar` | 局部变量或形参对应的栈槽/局部存储 |
| `Temp` | 三地址码临时结果 |
| `Label` | 基本块标签 |

### 5.3 Instruction 层

所有指令继承 `Instruction`，普通指令默认不是终结指令，`Branch`、`CondBranch`、`Return` 为终结指令。

| 指令 | 说明 |
|------|------|
| `BinaryOp` | `ADD/SUB/MUL/DIV/MOD` 算术运算 |
| `Compare` | `LT/GT/LE/GE/EQ/NE` 比较运算，结果为 `int` 布尔值 |
| `UnaryOp` | `NEG/NOT` 一元运算 |
| `LoadImm` | 加载整数立即数到临时值 |
| `Alloca` | 声明局部变量/形参存储 |
| `Load` | 从局部或地址读取值 |
| `Store` | 将值写入局部或地址 |
| `Move` | 显式值拷贝，主要用于 Phi 降级后的普通拷贝 |
| `Phi` | SSA 控制流汇合节点，记录不同前驱块传入的值 |
| `GlobalAddr` | 获取全局变量地址 |
| `Call` | 函数调用，`void` 调用无结果值 |
| `Branch` | 无条件跳转 |
| `CondBranch` | 条件跳转，条件按非零为真解释 |
| `Return` | 函数返回 |

### 5.4 Block 层

| 类 | 字段 | 说明 |
|----|------|------|
| `BasicBlock` | `Label label`, `List<Phi> phis`, `List<Instruction> instructions`, `Instruction terminator` | CFG 原生基本块，Phi、普通指令和终结指令分离保存 |
| `Function` | `String name`, `Type returnType`, `List<LocalVar> parameters`, `List<BasicBlock> blocks`, `BasicBlock entryBlock`, `Map<String, LocalVar> locals` | 函数级 IR |
| `Module` | `List<GlobalVar> globals`, `List<Constant> globalConstants`, `List<Function> functions`, `Function mainFunction` | 整个编译单元的 IR |

`BasicBlock.instructions()` 只返回普通指令，不包含 Phi 和终结指令；`BasicBlock.terminator()` 返回块末尾的 `Branch`、`CondBranch` 或 `Return`；需要完整顺序遍历时使用 `BasicBlock.allInstructions()`。

### 5.5 AST 到 IR 构建

`IRBuilder` 负责把语义分析后的 AST 转为 `Module`。构建时依赖 `SemanticResult` 提供的符号绑定，避免在 IR 阶段重新按名字查找变量或函数。

主要封装方法包括：

- `newTemp(Type)`、`newLabel(String)`、`newLocal(String, boolean)`：统一创建 IR 值。
- `appendBlock(String)`、`setCurrentBlock(BasicBlock)`、`emit(Instruction)`：管理基本块和指令追加。
- `emitExpr(Expr)`：表达式生成，处理算术、比较、一元、函数调用和短路逻辑。
- `emitCondExpr(Expr, trueTarget, falseTarget)`：条件表达式生成，直接为 `if` / `while` 条件中的 `&&`、`||`、`!` 生成短路控制流，避免先物化为布尔临时值。
- `emitStmt(Stmt)`：语句生成，处理声明、赋值、分支、循环、跳转和返回。
- `emitBranchIfOpen(Label)`：为尚未终结的块补充跳转。

### 5.6 `-opt` 优化管线

优化入口为 `toyc.opt.Optimizer`。当前优化管线只使用主 IR，不引入旁路 IR 或临时 SSA 表示。整体流程如下：

1. `ControlFlowSimplifier.simplify(function)`：先清理明显冗余的控制流，得到较稳定的 CFG。
2. `Mem2Reg.promote(function)`：基于支配关系和支配边界，将可提升的局部变量从 `Alloca/Load/Store` 形式提升为 SSA 值，并在控制流汇合处插入 `Phi`。
3. `SmallFunctionInliner.inline(module)`：在模块范围内展开满足条件的小函数调用，减少循环热点中的调用开销，并为后续局部值优化暴露更多常量折叠机会。
4. `PhiLowerer.lower(function)`：将 `Phi` 降为前驱块中的 `Move`，保证后端不直接处理 Phi。
5. 迭代执行 `LocalValueOptimizer`、`LinearExpressionOptimizer`、`LocalCse`、`LoopInvariantCodeMotion`、`GlobalLoopStorePromotion`、`ControlFlowSimplifier` 和 `DeadStoreEliminator`，直到没有新的变化。
6. `DeadFunctionEliminator.eliminate(module)`：删除从 `main` 出发不可达的函数，清理小函数内联后不再被调用的函数体。

也就是说，优化阶段先把局部变量读写转换为更清晰的 SSA 值关系，再利用该值流形态进行小函数内联，然后把机器无法直接表达的 Phi 降为普通拷贝，最后持续清理常量、线性表达式、重复表达式、循环内不变量、死代码和冗余控制流。

### 5.7 CFG 分析与控制流简化

`ControlFlowGraph` 根据每个基本块的 `terminator` 构建后继、前驱和入口可达块集合。若跳转目标不存在，会立即报错，避免后续生成错误汇编。

`ControlFlowSimplifier` 支持以下化简：

- 折叠常量条件分支，例如 `if 0` 直接跳到 false 分支。
- 将真/假目标相同的条件分支改为无条件跳转。
- 旁路只包含无条件跳转的中转块。
- 合并单前驱、单后继的线性基本块。
- 删除入口不可达的基本块。

该 pass 内部会迭代到收敛，因为一次分支折叠可能继续产生不可达块、空跳转块或新的线性合并机会。

### 5.8 Mem2Reg 与 Phi 降级

朴素 IR 会把局部变量表示成内存槽：

```text
Alloca x
Store value, x
Load t, x
```

这种形式直观但冗余。`Mem2Reg` 会把可安全提升的局部变量改写成 SSA 值：

```text
Store value, x  -> 更新 x 的当前 SSA 值
Load t, x       -> 直接替换为 x 的当前 SSA 值
```

当多个控制流路径汇合时，`Mem2Reg` 使用 `Phi` 合并不同前驱传入的值。例如：

```text
then: x = 1
else: x = 2
join: use x
```

会在 `join` 处形成类似：

```text
x3 = phi [x1, then], [x2, else]
```

由于真实机器没有 Phi 指令，`PhiLowerer` 会把 Phi 改写为前驱块里的 `Move`：

```text
then: move x3, x1; jump join
else: move x3, x2; jump join
join: use x3
```

当前 ToyC 没有指针，局部变量通常只通过直接的 `Load/Store LocalVar` 访问，因此 Mem2Reg 能覆盖大部分局部变量读写场景。全局变量和不明确地址不参与该提升。

### 5.9 小函数内联

`SmallFunctionInliner` 是一个模块级优化 pass，运行在各函数完成 `ControlFlowSimplifier` 和 `Mem2Reg` 之后、`PhiLowerer` 之前。这样安排的原因是：`IRBuilder` 初始生成的函数体通常包含 `Alloca/Load/Store` 形式的局部变量访问，而 `Mem2Reg` 会先把大部分局部变量读写提升为更直接的 SSA 值关系，使小函数体更接近纯表达式或简单内存更新，便于安全展开。

当前内联策略偏保守，只处理满足以下条件的被调函数：

- 被调函数存在，且不是当前调用者自身。
- 实参与形参数量一致。
- 函数体只有一个基本块，且该基本块不含 `Phi`。
- 终结指令必须是 `Return`。
- 普通指令数不超过固定阈值，目前为 8 条。
- 函数体不包含对自身的递归调用。
- 普通指令只能是 `LoadImm`、`Move`、`UnaryOp`、`BinaryOp`、`Compare`、`GlobalAddr`、`Load`、`Store` 这些可直接克隆的指令，不内联含内部函数调用或复杂控制流的函数。

展开时，内联器会将形参替换为调用点实参，并为被调函数中的每个 `Temp` 结果在调用者函数内创建新的临时值，避免多个调用点共享同一个 IR 值或产生重复定义。若被调函数返回 `int` 值，则在调用点追加一次 `Move`，把展开后的返回值写入原 `Call` 的结果临时值；若被调函数是 `void`，则直接移除原调用并保留展开后的副作用指令。

该 pass 主要针对两类性能热点：一类是循环中频繁调用的纯计算小函数，例如多个整数参数组合成一个表达式；另一类是简单的全局状态更新函数，例如读取全局变量、计算新值后写回。递归函数、多基本块函数和包含内部调用的函数不会被展开，以避免代码膨胀和控制流复杂化。

### 5.10 局部公共子表达式消除

`LocalCse` 负责基本块内的轻量级公共子表达式消除。它不做跨基本块数据流分析，也不试图处理可能受内存副作用影响的 `Load` 指令，而是只记录当前基本块内已经出现过的纯计算表达式和全局地址计算：

- `UnaryOp`：相同一元运算和相同操作数。
- `BinaryOp`：相同二元运算和相同左右操作数。
- `Compare`：相同比较谓词和相同左右操作数。
- `GlobalAddr`：相同全局变量地址。

当后续遇到完全相同的表达式时，该 pass 不再保留重复计算，而是将其替换为一次 `Move`，把已有结果拷贝到当前指令原本的结果临时值。例如：

```text
t1 = mul x, 3
t2 = mul x, 3
```

会被改写为：

```text
t1 = mul x, 3
t2 = move t1
```

随后下一轮 `LocalValueOptimizer` 会继续做拷贝传播和死临时值删除，将 CSE 产生的 `Move` 和被替换表达式的冗余结果清理掉。这样的分工使 `LocalCse` 保持很小的职责边界：只识别重复表达式，不重复实现常量折叠、代数化简或死代码删除。

该 pass 对小函数内联后的代码尤其有用。内联会把被调函数体复制到调用点，容易在同一基本块内产生重复的 `GlobalAddr` 或相同算术表达式；局部 CSE 可以在不引入复杂别名分析的前提下清理这些冗余。

### 5.11 线性表达式规约

`LinearExpressionOptimizer` 负责基本块内的简单线性表达式规约。它只处理“单个基值加减若干常量”的表达式链，例如：

```text
t1 = add x, 6
t2 = sub t1, 4
t3 = add t2, 30
```

会被规约为：

```text
t3 = add x, 32
```

该 pass 的目标是清理小函数内联后产生的常量加减链。例如 `mix(i, 2, 3, 4, 5, 6, 7, 8, 9)` 内联后会形成一串加减常量表达式，规约后可以变成 `i + 42`，显著减少循环体中的算术指令和中间临时值。

为保证语义安全，当前实现只识别一个基值和一个常量偏移，不会把 `x + x` 这类表达式错误当成 `x + 常量` 处理；遇到两个不同基值参与的加减表达式时会放弃优化。

### 5.12 循环内不变量与全局写提升

`LoopInvariantCodeMotion` 是一个窄版循环不变量外提 pass。它只识别 IRBuilder 生成的简单 while 形态，并且只把循环体中的 `LoadImm` 和 `GlobalAddr` 提升到循环前驱块中。该 pass 不移动 `Load`、`Store`、`Call`、`Div`、`Mod` 等可能涉及副作用、异常或复杂语义的指令，因此实现范围较小但风险可控。

`GlobalLoopStorePromotion` 针对简单全局变量累加循环做进一步优化。它匹配如下保守条件：

- 循环为单条件块、单循环体块、单退出块的 while 形态。
- 循环体内没有函数调用。
- 循环体内只存在对同一个全局地址的一次读取和一次写回，没有其他不明确 `Store`。
- 全局地址已经在循环前驱块中可用。

匹配成功后，优化会在循环前读取全局变量值，在循环体内更新一个临时累加值，并在循环退出块中写回全局变量。这样可以把原本每轮执行的全局 `Load/Store` 降为循环前一次读取和循环后一次写回，适合 `g = g + expr` 这类热点循环。

### 5.13 局部值优化与死写删除

`LocalValueOptimizer` 负责普通 value 层面的清理，不再维护 `LocalVar -> value` 这类内存缓存。它主要处理：

- `LoadImm` 常量传播。
- `Move` 拷贝传播。
- 一元、二元、比较指令的常量折叠。
- 简单代数化简，例如 `x + 0`、`x * 1`、`x * 0`、`x / 1`、`x % 1`。
- 删除无副作用且结果未使用的临时值定义。

`DeadStoreEliminator` 基于 CFG 对 `LocalVar` 做反向活跃性分析，删除后续不再读取的局部变量写入。它只删除 `Store value, LocalVar` 这种局部死写，不删除全局变量写入和不明确地址写入。

`DeadFunctionEliminator` 在函数级优化完成后运行，从 `main` 函数出发沿 `Call` 指令构建可达函数集合，并删除不可达函数。该 pass 主要用于清理小函数内联后不再被调用的辅助函数，减少最终发射的汇编体积。

### 5.14 当前状态与限制

当前中端已经具备 CFG-native IR、Mem2Reg、Phi 插入与降级、小函数内联、局部公共子表达式消除、线性表达式规约、窄版循环不变量外提、简单全局写提升、基础值优化和 DSE。优化管线结构已经能承载后续更强的 SSA 优化。

但当前还没有实现 SCCP、GVN、循环不变量外提、强度削弱等更激进的中端优化；后端也仍采用栈帧式发射策略，`Temp` 和未提升的 `LocalVar` 默认会分配栈槽。因此性能瓶颈仍主要来自后端大量 `lw/sw` 和缺少寄存器分配。后续优化重点可以放在 SSA 上的全局优化和后端寄存器分配。

---

## 六、后端与目标代码生成

### 6.1 RISC-V 发射器设计

`RiscVEmitter` 基于 IR Visitor 遍历 `IRProgram`。后端输入为 `IRBuilder` 生成的非 SSA 三地址码 IR，输出为可写入 stdout 的 RISC-V 32 文本汇编。

当前后端采用朴素但稳定的**栈帧式发射策略**：

- 每个 `LocalVar` 和 `Temp` 都分配一个函数内栈槽。
- 每条指令使用 `t0`、`t1`、`t2` 作为短生命周期工作寄存器完成计算。
- 指令结果立即写回对应栈槽，不依赖活跃变量分析或寄存器分配。
- 函数入口建立栈帧，保存 `ra` 和 `s0`；函数返回前恢复栈帧并 `ret`。
- 栈帧大小按 16 字节对齐，便于函数调用时保持调用约定所需的栈对齐。

当前发射流程：

1. 检查模块中存在 `int main()` 对应的 IR 函数。
2. 若存在全局变量，先输出 `.data` 段和各全局变量的 `.word` 初始值。
3. 输出 `.text` 段。
4. 顺序发射模块中的所有函数，而不是只发射 `main`。
5. 对每个函数预扫描局部变量和临时值，计算栈槽与栈帧大小。
6. 输出函数标签、序言、参数落栈逻辑。
7. 顺序遍历基本块和指令，生成对应汇编。
8. `Return` 将返回值装入 `a0`，恢复 `ra`、`s0` 和 `sp` 后返回。

### 6.2 当前支持的 IR 指令

当前后端已覆盖 `IRBuilder` 目前会生成的主要 IR 指令：

| IR 指令 | RISC-V 发射 |
|---------|-------------|
| `LoadImm` | `li` |
| `Alloca` | 不直接发射指令；其对应存储在函数栈帧布局阶段分配 |
| `Load` | 从局部栈槽或地址寄存器指向的内存中 `lw` |
| `Store` | 向局部栈槽或地址寄存器指向的内存中 `sw` |
| `GlobalAddr` | 使用 `la` 获取全局变量地址 |
| `Call` | 按简化 RISC-V 调用约定传参、`call`、保存返回值 |
| `Return` | 将返回值加载到 `a0`，随后执行函数尾声和 `ret` |
| `UnaryOp.NEG` | `neg` |
| `UnaryOp.NOT` | `seqz` |
| `BinaryOp.ADD/SUB/MUL/DIV/MOD` | `add/sub/mul/div/rem` |
| `Compare.LT/GT/LE/GE/EQ/NE` | `slt`、`xori`、`xor`、`seqz`、`snez` 组合 |
| `Branch` | `j` |
| `CondBranch` | `bnez` + `j` |

基本块标签来自 IR `Label`，发射时会转换为汇编标签文本。函数入口块复用函数标签，非入口块单独输出标签。

### 6.3 栈帧与局部存储

当前后端不做真正的寄存器分配，而是把 IR 值统一落到栈上：

- 函数的 `locals()` 中包含形参和局部变量，均分配固定栈槽。
- 每条有 `Temp` 结果的指令也分配一个固定栈槽。
- `Alloca` 在 IR 中保留局部存储语义，但后端只在栈帧布局阶段处理，不单独输出汇编。
- 访问栈槽时以 `s0` 作为当前函数帧基址。
- 对超过 RISC-V `lw/sw/addi` 12 位有符号立即数范围的偏移，后端会先用 `li` 加载偏移，再通过 `add` 计算地址。

该设计生成的汇编较冗余，但非常适合当前阶段验证 AST → IR → ASM 全链路正确性。后续可在此基础上加入活跃变量分析、寄存器分配和栈槽复用。

### 6.4 函数调用约定

当前实现采用简化 RISC-V 调用约定：

- 前 8 个实参使用 `a0` 到 `a7` 传递。
- 第 9 个及之后实参由调用者临时压到栈上传递。
- 被调用函数入口将寄存器参数和栈上传入参数保存到本函数形参栈槽。
- 函数返回值通过 `a0` 传回。
- 每个函数保存并恢复 `ra`、`s0`，因此普通函数调用和递归调用具备基础支持。

例如 ToyC 代码：

```c
int g = 5;

int add(int a, int b) {
    return a + b + g;
}

int main() {
    int i = 0;
    int sum = 0;
    while (i < 5) {
        i = i + 1;
        if (i == 3) {
            continue;
        }
        sum = sum + add(i, 1);
    }
    return sum;
}
```

会生成形如下面的汇编结构：

```asm
.data
.globl g
g:
    .word 5
.text
.globl add
add:
    addi sp, sp, -48
    sw ra, 44(sp)
    sw s0, 40(sp)
    mv s0, sp
    ...
    call ...
    ...
.globl main
main:
    addi sp, sp, -96
    sw ra, 92(sp)
    sw s0, 88(sp)
    mv s0, sp
    ...
while_cond_2:
    ...
    call add
    ...
    ret
```

### 6.5 当前限制与待办事项

从能力扩展角度，后续工作建议按以下顺序推进：

1. **负向测试体系**：覆盖重复 `main`、缺失 `main`、变量未声明、`break/continue` 在循环外、`const` 被赋值、函数参数数量不匹配等期望编译失败的场景。
2. **无初始化变量**：支持 `int x;`，局部变量和全局变量默认初始化为 `0`。
3. **多变量声明**：支持 `int a = 1, b = 2;` 与 `const int x = 1, y = 2;`。
4. **数组基础能力**：支持一维全局数组、一维局部数组、数组下标访问，后续再扩展数组参数和多维数组。
5. **内建 I/O 函数**：按实验需求支持 `getint()`、`putint(x)`、`putch(x)` 等运行时函数。
6. **调用约定增强**：进一步贴近 RISC-V ABI，完善 caller-saved/callee-saved 寄存器、栈上传参布局和调用前栈对齐。

从编译优化角度，后续工作建议按以下顺序推进：

1. **常量折叠**：将 `1 + 2 * 3` 等可静态计算的 IR 表达式提前折叠为常量。
2. **代数化简**：处理 `x + 0`、`x * 1`、`x * 0`、`x / 1` 等简单等价替换。
3. **死代码删除**：删除终结指令后的不可达指令，以及没有前驱的不可达基本块。
4. **简单常量传播**：识别局部变量只被常量赋值且未修改的场景，将后续 load 替换为常量。
5. **控制流简化**：删除空跳转块、合并单前驱/单后继基本块、消除 `br L; L:` 形式的冗余跳转。
6. **栈槽复用与寄存器分配**：当前所有 IR 值落栈，后续可先做临时栈槽复用，再引入活跃变量分析和线性扫描寄存器分配。

其他横向整理事项：

- 类型抽象统一：当前前端/语义分析中部分类型信息仍使用 `FuncDef.Type` 表示。代码生成与 IR 相关的新代码先统一使用 `toyc.common.Type`，后续在不影响其他组员工作的前提下，再将 `FuncDef.Type`、`Symbol`、`FunctionSymbol`、`SemanticResult` 等位置逐步迁移到全局公共类型枚举。
- 后端验证扩充：已接入基于 JUnit、RISC-V 交叉 GCC 和 `qemu-riscv32` 的 smoke 运行测试，后续可继续扩充更多语言特性用例和错误用例。

---

## 七、测试与真实运行验证

### 7.1 测试分层

当前测试分为三层：

1. **汇编快照测试**：将 ToyC 源码编译为汇编，并与 `*.expected.s` 文件逐字节比较，防止后端输出意外变化。
2. **QEMU 运行测试**：将 ToyC 源码编译为汇编，使用 RISC-V 交叉 GCC 链接为 RV32 ELF，再通过 `qemu-riscv32` 执行并检查退出码。
3. **负向编译测试**：将期望非法的 ToyC 源码送入编译流程，检查编译失败信息是否包含期望错误文本。

相关测试入口为 `src/test/java/toyc/SmokeCompilationTest.java`，运行方式为：

```bash
mvn test
```

### 7.2 Smoke 用例约定

Smoke 测试资源位于 `src/test/resources/smoke/`：

| 文件 | 说明 |
|------|------|
| `*.tc` | ToyC 源程序 |
| `*.expected.s` | 可选，期望汇编快照 |
| `*.expected.exit` | 可选，QEMU 执行后的期望进程退出码 |

测试类会自动发现：

- 所有 `*.expected.s`，查找同名 `.tc` 后执行汇编快照测试。
- 所有 `*.expected.exit`，查找同名 `.tc` 后执行真实运行测试。

例如：

```text
complex_control_call_global.tc
complex_control_call_global.expected.s
complex_control_call_global.expected.exit
```

表示该用例既参与汇编快照测试，也参与 QEMU 运行测试。

### 7.3 RV32 运行时入口

ToyC 后端只发射用户函数，包括 `main`，不发射进程入口 `_start`。为了让裸汇编能在 QEMU 用户态运行，测试资源中提供了最小运行时入口：

```asm
.section .text
.globl _start
_start:
    call main
    li a7, 93
    ecall
1:
    j 1b
```

该文件位于 `src/test/resources/runtime/start_rv32.S`。它调用 ToyC 生成的 `main`，随后使用 Linux RISC-V `exit` syscall 将 `main` 返回值作为进程退出码。

### 7.4 错误用例约定

负向编译测试资源位于 `src/test/resources/errors/`：

| 文件 | 说明 |
|------|------|
| `*.tc` | 期望编译失败的 ToyC 源程序 |
| `*.expected.error` | 期望错误信息片段 |

测试类会自动发现所有 `*.expected.error`，查找同名 `.tc` 后执行编译失败断言。示例：

```text
multi_main.tc
multi_main.expected.error
```

其中 `multi_main.expected.error` 可以只写关键错误片段，例如：

```text
duplicate top-level declaration: main
```

只要实际异常消息包含该片段，测试即通过。

### 7.5 外部工具依赖

QEMU 运行测试依赖以下命令：

- `riscv64-unknown-elf-gcc`
- `qemu-riscv32`

测试中使用的链接命令形态为：

```bash
riscv64-unknown-elf-gcc \
  -march=rv32im \
  -mabi=ilp32 \
  -nostdlib \
  -nostartfiles \
  src/test/resources/runtime/start_rv32.S \
  generated.s \
  -o generated.elf
```

若缺少上述外部命令，JUnit 会跳过 QEMU 运行测试；汇编快照测试仍可执行。由于进程退出码范围为 `0..255`，`*.expected.exit` 中的期望值应保持在该范围内。

### 7.6 临时文件组织

QEMU 运行测试会保留每个用例生成的汇编和 ELF 文件，便于排查失败原因。临时文件统一放在系统临时目录下的 `toyc-compiler` 子目录中，每次 JUnit 运行按时间戳创建独立目录：

```text
/tmp/toyc-compiler/
  20260623-161809-087174292/
    main_return_0/
      main_return_0.s
      main_return_0.elf
    complex_control_call_global/
      complex_control_call_global.s
      complex_control_call_global.elf
```

目录名使用 `yyyyMMdd-HHmmss-nnnnnnnnn` 格式，最后一段为纳秒精度字段。测试不会自动删除这些文件，方便在失败后直接使用汇编器、`objdump` 或 `qemu-riscv32` 复现问题。
