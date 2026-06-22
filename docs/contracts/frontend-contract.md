# Frontend Contract

## TokenKind Categories

`TokenKind` lives in `src/common/token.h`.

- End marker: `Eof`.
- Literals and names: `Identifier`, `IntegerLiteral`.
- Keywords: `int`, `void`, `const`, `if`, `else`, `while`, `break`, `continue`, `return`.
- Delimiters: `(`, `)`, `{`, `}`, `,`, `;`.
- Operators: `+`, `-`, `*`, `/`, `%`, `!`, `<`, `>`, `=`, `<=`, `>=`, `==`, `!=`, `&&`, `||`.

Every `Token` stores its `TokenKind`, original lexeme text, and a 1-based `SourceLocation`.

## TokenStream

`TokenStream` lives in `src/common/token_stream.h` and owns a vector of tokens ending in `Eof`.

- `peek(offset = 0)` returns the current token or a lookahead token without consuming it.
- `consume()` returns the current token and advances unless the stream is already at `Eof`.
- `match(kind)` consumes and returns `true` when the current token has the requested kind.
- `expect(kind, message)` consumes the requested token or throws `ParseError` with line, column, expected kind, actual kind, and actual lexeme.

The interface is intended for recursive-descent parser code and keeps parser diagnostics location-aware from the first parser stage.

## Lexer Behavior

The lexer accepts source text as `std::string_view` through `Lexer` or the `lex()` helper and returns `std::vector<Token>`.

- Identifiers match `[_A-Za-z][_A-Za-z0-9]*`.
- Decimal integer literals are digit sequences and preserve their original text.
- Multi-digit decimal literals starting with `0` are lexical errors.
- Whitespace is ignored.
- `//` comments are ignored through the next newline.
- `/* ... */` comments are ignored through the first closing `*/`.
- Unknown characters and unterminated block comments raise `LexError` with line and column.
- Two-character operators use longest-match recognition for `<=`, `>=`, `==`, `!=`, `&&`, and `||`.
- The final token is always `Eof`.

## Negative Number Policy

The lexer emits `-` as `TokenKind::Minus` and emits following digits as an unsigned decimal `IntegerLiteral`. Examples: `-42` tokenizes as `Minus`, `IntegerLiteral("42")`; `a+-1` tokenizes as `Identifier("a")`, `Plus`, `Minus`, `IntegerLiteral("1")`.

Negative values belong to future unary expression parsing. The lexer never folds a sign into an integer literal.

## Expression Parser Boundary

Member 1 owns the expression parser entry point:

- `parseExpr()` will parse ToyC expressions after the shared AST interface is frozen.
- Member 1 owns expression AST implementation after that interface is agreed.

Member 2 calls `parseExpr()` from declaration initializers, assignments, conditions, return statements, and function argument parsing. Member 2 owns declaration, statement, function, global pre-collection, and constant-evaluation integration around that expression boundary.
