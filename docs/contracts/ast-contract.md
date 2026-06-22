# AST Contract

## Public Model

The AST data model lives in `src/ast/ast.h` under `toyc::ast`. `CompUnit` is the parser
output and the sole root type consumed by Sema.

The node hierarchy is:

```text
Node
|- Decl: VarDecl, ConstDecl, Param, FuncDef
|- Stmt: BlockStmt, EmptyStmt, ExprStmt, AssignStmt, DeclStmt,
|        IfStmt, WhileStmt, BreakStmt, ContinueStmt, ReturnStmt
`- Expr: IntLiteralExpr, DeclRefExpr, CallExpr, UnaryExpr, BinaryExpr

CompUnit derives from Node.
```

Every node stores a `SourceRange`. Ranges are half-open `[begin, end)`, and line and
column values are 1-based. `AssignStmt::targetRange` records the assigned identifier
separately from the complete statement range.

## Ownership

Child nodes use `std::unique_ptr`. Node collections use
`std::vector<std::unique_ptr<T>>`. `CompUnit` owns all top-level declarations and thus
owns the complete tree. `Node` has a virtual destructor for polymorphic ownership.

The aliases `ExprPtr`, `StmtPtr`, and `DeclPtr` are the standard pointer types for parser
and Sema interfaces.

## Stored Syntax

- Identifiers and integer spellings use `std::string`.
- `FuncDef` stores `TypeKind`, name, parameters, and body.
- Variable and constant declarations always carry an initializer expression.
- `ReturnStmt::value` and `IfStmt::elseBranch` use a null pointer for absent syntax.
- Unary and binary operators use the existing `toyc::TokenKind` values.
- `DeclStmt` owns a local `VarDecl` or `ConstDecl`; the parser enforces this restriction.
- Top-level `CompUnit::declarations` contains `VarDecl`, `ConstDecl`, or `FuncDef`; the
  parser enforces this restriction.

## Phase Boundary

The parser stores syntax and source ranges. The AST contains no symbol pointers, resolved
types, constant values, IR value IDs, or basic-block IDs. Sema stores binding and type
results in a semantic model keyed by AST identity. IR generation reads the AST and the
semantic model without modifying either input.
