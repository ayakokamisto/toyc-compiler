# Sema Input Contract

## Input and Output Boundary

Sema consumes Parser output through this phase boundary:

```text
Input:
const ast::CompUnit& unit

Output:
SemanticModel
std::vector<Diagnostic>
```

`Parser::parseCompUnit()` always returns a `std::unique_ptr<ast::CompUnit>`. Syntax errors
may leave a recovered, incomplete AST. The production Sema entry point accepts only an
AST produced without Error-level Parser diagnostics. Driver checks `Parser::hasError()`
and stops before Sema when syntax errors exist.

## Sema Responsibilities

Sema performs the following work:

- Maintain global and nested scopes.
- Bind each `DeclRefExpr` to a declaration or symbol.
- Bind each `CallExpr` to a `FuncDef` or function symbol.
- Verify that identifiers are used after declaration.
- Reject duplicate declarations in one scope.
- Implement inner-scope shadowing of variables, constants, and parameters.
- Verify that `main` exists, returns `int`, and has no parameters.
- Verify that function definitions occur only at global scope.
- Permit calls to functions already declared earlier in source order and permit recursion.
- Verify function argument counts.
- Restrict `break` and `continue` to `while` bodies.
- Reject assignment to a constant.
- Verify that every possible path in an `int` function returns an `int` value.
- Require value-free return statements in `void` functions.
- Reject use of a `void` call in a value context.
- Evaluate constant expressions.
- Restrict constant initializers to permitted constant-expression components.
- Check signed 32-bit integer ranges and division or remainder by zero.

## AST Field Guide

Field names below match `src/ast/ast.h`.

| AST node | Fields read by Sema | Semantic purpose |
|---|---|---|
| `CompUnit` | `declarations` | Build the global symbol table and analyze in source order |
| `VarDecl` / `ConstDecl` | `name`, `initializer`, `range` | Declare, validate initialization and constants, report ranges |
| `FuncDef` | `returnType`, `name`, `parameters`, `body`, `range` | Function symbol, function scope, return analysis |
| `Param` | `name`, `range` | Parameter symbols |
| `BlockStmt` | `statements`, `range` | Create and leave a nested scope |
| `DeclStmt` | `declaration`, `range` | Analyze a local declaration |
| `AssignStmt` | `target`, `targetRange`, `value`, `range` | Bind the left side, enforce const rules, check the value |
| `DeclRefExpr` | `name`, `range` | Name lookup and binding |
| `CallExpr` | `callee`, `arguments`, `range` | Function lookup, source order, arity and return type |
| `UnaryExpr` | `op`, `operand`, `range` | Type checking, constant evaluation and truth normalization |
| `BinaryExpr` | `op`, `left`, `right`, `range` | Type checking, constant evaluation and logical rules |
| `IfStmt` | `condition`, `thenBranch`, `elseBranch`, `range` | Condition and control-flow analysis |
| `WhileStmt` | `condition`, `body`, `range` | Condition, loop depth and control flow |
| `BreakStmt` / `ContinueStmt` | `range` | Loop-depth validation |
| `ReturnStmt` | `value`, `range` | Return type and path completeness |

All AST nodes inherit `Node::range`. `EmptyStmt` and `ExprStmt` are analyzed through their
`range` and, for `ExprStmt`, `expression` fields.

## Recommended SemanticModel Side Tables

The concrete header and supporting symbol/type classes remain a member-two deliverable.
A recommended query surface is:

```cpp
class SemanticModel {
public:
    const Symbol* lookupBinding(const ast::DeclRefExpr& expr) const;
    const FunctionSymbol* lookupCallee(const ast::CallExpr& call) const;
    Type getExprType(const ast::Expr& expr) const;
    std::optional<std::int32_t> getConstantValue(const ast::Expr& expr) const;
};
```

Use AST node addresses as side-table keys. `CompUnit` owns its tree through
`std::unique_ptr`, so node addresses stay stable throughout one compilation. Sema keeps
symbols, types, constant values, and IR identifiers outside AST nodes. `SemanticModel`
must outlive the corresponding `IRGenerator` call.

## Items to Freeze Next

- Member two must freeze the Sema public header path and concrete class names before
  publishing the implementation.
- Members two and three must agree on the exact `IRGenerator` queries into
  `SemanticModel`.
- The team must confirm compile-time evaluation rules for global variable initializers
  before Sema implementation.
- Member four owns the separate RISC-V32 ABI contract.
