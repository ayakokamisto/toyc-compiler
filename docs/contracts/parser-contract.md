# Parser Contract

## Public Entry Points

`toyc::parser::Parser` lives in `src/parser/parser.h` and borrows a
`toyc::TokenStream` for its lifetime.

```cpp
std::unique_ptr<ast::CompUnit> parseCompUnit();
std::unique_ptr<ast::Expr> parseExpr();
```

`parseCompUnit()` parses the complete source and checks `Eof`. `parseExpr()` parses the
expression body only. Its caller checks the contextual terminator: `;`, `)`, `,`, `}`,
or `Eof`. An expression parser returns when the current token does not continue the
expression grammar.

## Grammar Decisions

- Top-level `const int` starts a `ConstDecl`.
- Top-level `int Identifier =` starts a `VarDecl`.
- Top-level `int Identifier (` and `void Identifier (` start a `FuncDef`.
- Function definitions are accepted only at global scope.
- At statement start, `Identifier` followed by `TokenKind::Equal` starts an
  `AssignStmt`. `Equal` is the current TokenKind name for the grammar's assignment token.
- Other expression starts produce an `ExprStmt`.
- `parseIfStmt()` checks for `else` immediately after its then branch, binding the token
  to the nearest incomplete `if`.

## Statement Parsing

`parseStmt()` applies the following ordered dispatch:

1. `{` starts a `BlockStmt`.
2. `const` or `int` starts a local declaration wrapped in `DeclStmt`.
3. `if` starts an `IfStmt`.
4. `while` starts a `WhileStmt`.
5. `break` starts a `BreakStmt`.
6. `continue` starts a `ContinueStmt`.
7. `return` starts a `ReturnStmt` with an optional expression.
8. `;` produces an `EmptyStmt`.
9. `Identifier` followed by `TokenKind::Equal` starts an `AssignStmt`.
10. Every remaining expression start is parsed as `ExprStmt` followed by `;`.

Local declarations reuse `parseConstDecl()` and `parseVarDecl()`. Parser records their
syntax and source ranges. Sema owns duplicate-declaration, shadowing, constant-
initializer, and assignment-target checks.

`AssignStmt` has the fixed form `Identifier = Expr ;`. It stores the target spelling,
the target token range, the value expression, and the complete statement range.

`return ;` produces `ReturnStmt` with an empty value. `return Expr ;` stores the parsed
expression. Sema checks function return type compatibility and all-path return rules.

## Control-flow Statement Parsing

`parseIfStmt()`, `parseWhileStmt()`, `parseBreakStmt()`, and `parseContinueStmt()` build
syntax-only control-flow nodes.

An if statement follows `if ( Expr ) Stmt (else Stmt)?`. The then branch and optional
else branch both use ordinary `Stmt` parsing. `elseBranch` is null when the source omits
`else`. `parseIfStmt()` checks for `else` immediately after parsing its then branch, so an
`else` binds to the nearest incomplete if.

A while statement follows `while ( Expr ) Stmt`; its body may be a single statement, a
block, another loop, or an if statement. Break and continue follow `break ;` and
`continue ;`. Parser accepts these nodes in every statement context. Sema checks whether
they appear inside a loop and checks all condition and reachability constraints.

`IfStmt::range` begins at `if` and ends at the else branch when present, otherwise at the
then branch. `WhileStmt::range` begins at `while` and ends at its body. Break and continue
ranges include their terminating semicolon.

With these control-flow productions, Parser covers every ToyC grammar production. Sema
remains responsible for name binding, scope rules, types, constant-expression validity,
function return rules, loop context, and other semantic constraints.

## Expression Parsing

The parser uses recursive descent with one function per precedence level. Binary levels
use loops to build left-associative trees. `parseUnaryExpr()` recursively handles unary
`+`, `-`, and `!`. `parsePrimaryExpr()` distinguishes a declaration reference from a call
by checking for `(` after an identifier.

Every binary operator is left-associative. `&&` has higher precedence than `||` because
`parseLogicalOrExpr()` parses each operand through `parseLogicalAndExpr()`.

The lexer keeps `-` separate from integer tokens. The expression `-2` therefore has the
fixed AST form:

```text
UnaryExpr(Minus, IntLiteralExpr("2"))
```

ToyC expression parsing follows C operator precedence. From highest to lowest, the
frozen precedence layers are:

```text
Primary
Unary: + - !
Multiplicative: * / %
Additive: + -
Relational: < <= > >=
Equality: == !=
LogicalAnd: &&
LogicalOr: ||
```

The recursive-descent call chain is:

```text
parseLogicalOrExpr()
  -> parseLogicalAndExpr()
  -> parseEqualityExpr()
  -> parseRelationalExpr()
  -> parseAdditiveExpr()
  -> parseMultiplicativeExpr()
  -> parseUnaryExpr()
  -> parsePrimaryExpr()
```

`parseEqualityExpr()` returns `std::unique_ptr<ast::Expr>`. It owns `==` and `!=`.
`parseRelationalExpr()` owns `<`, `<=`, `>`, and `>=`. This precedence split governs the
implementation when the compact grammar groups all six operators under `RelExpr`.

Logical `&&` and `||` remain `BinaryExpr` syntax nodes. IR generation supplies their
short-circuit control flow.

## Diagnostics and Recovery

Parser diagnostics use `std::vector<Diagnostic>`. `hasError()` reports whether an error
was collected. The implementation may use the existing `ParseError` from
`TokenStream::expect()` internally and converts caught failures into `Diagnostic` values.

Statement recovery synchronizes at `;` or `}`. Top-level recovery synchronizes at
`const`, `int`, `void`, or `Eof`. Every recovery path must consume input or reach `Eof` so
that malformed input cannot cause an infinite loop.

## Parser P5 Delivery Status

Parser covers every ToyC grammar production: compilation units, declarations, functions,
parameters, blocks, all statement forms, function calls, and the complete expression
precedence hierarchy.

Parser owns syntax recognition, syntax-only AST construction, source ranges, syntax
diagnostics, and bounded recovery. Scope, symbols, types, constants, control-flow
legality, return-path completeness, use of void values, and function declaration order
belong to Sema.

`parseCompUnit()` returns a recovered `std::unique_ptr<ast::CompUnit>` even when syntax
errors occur. The tree may omit malformed declarations or statements while preserving
later valid syntax. Callers inspect `hasError()` and `diagnostics()`. Driver must emit
Error-level Parser diagnostics and stop before Sema, IR generation, and CodeGen.

`parser_p5_tests` provides final regression coverage for a complete ToyC program,
expression shape and associativity, source ranges across all major node families,
top-level and statement recovery, and syntax/semantic responsibility separation. The
Sema-facing phase boundary is specified in `docs/contracts/sema-input-contract.md`.
