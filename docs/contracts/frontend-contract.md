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

The project uses negative-number policy A. The lexer always emits `-` as
`TokenKind::Minus`. An `IntegerLiteral` lexeme contains decimal digits only and never
contains a sign.

- `-2` becomes `Minus`, `IntegerLiteral("2")`.
- `a + -2` becomes `Identifier("a")`, `Plus`, `Minus`, `IntegerLiteral("2")`.
- `a--2` becomes `Identifier("a")`, `Minus`, `Minus`, `IntegerLiteral("2")`.

The parser's unary-expression layer interprets unary plus, unary minus, and logical not.

## Decimal Integer Range

The lexer recognizes valid decimal digit strings, rejects invalid leading zeroes, and
preserves the original lexeme. Sema performs signed 32-bit range checks during constant
evaluation.

- Ordinary positive integer constants range from `0` through `2147483647`.
- `-2147483648` is represented as
  `UnaryExpr(Minus, IntLiteralExpr("2147483648"))` and is valid.
- `2147483648` as a positive expression is a semantic error.
- `-2147483649` is a semantic error.

The lexer may emit `IntegerLiteral("2147483648")`; Sema determines validity from the AST
context.

## Frontend Responsibility Boundary

- The lexer produces a complete token sequence ending in `Eof`.
- Token locations use 1-based starting line and column numbers.
- `TokenStream` provides read-only lookahead, consumption, matching, and expected-token
  checks.
- The parser builds a syntax-only AST.
- Sema owns name binding, types, constant values, scopes, and control-flow legality in
  semantic side tables.
- IR generation reads the AST and semantic model and never mutates the AST.
- Code generation consumes an IR `Module` accepted by the IR verifier.

## Expression Parser Boundary

Member 1 owns the complete AST and parser. `parseExpr()` is the shared expression entry
used by declaration initializers, assignments, conditions, return statements, and call
arguments. Member 2 consumes the completed `ast::CompUnit` in Sema. The full ownership
boundary is frozen in `docs/team-plan.md`.
