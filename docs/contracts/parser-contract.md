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
