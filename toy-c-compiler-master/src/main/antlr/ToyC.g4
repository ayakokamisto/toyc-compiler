grammar ToyC;

@header {
package toyc.frontend.parser;
}

compUnit
    : (decl | funcDef)+ EOF
    ;

decl
    : constDecl
    | varDecl
    ;

constDecl
    : CONST INT ID ASSIGN expr SEMI
    ;

varDecl
    : INT ID ASSIGN expr SEMI
    ;

funcDef
    : (INT | VOID) ID LPAREN (param (COMMA param)*)? RPAREN block
    ;

param
    : INT ID
    ;

block
    : LBRACE stmt* RBRACE
    ;

stmt
    : block
    | SEMI
    | expr SEMI
    | ID ASSIGN expr SEMI
    | decl
    | IF LPAREN expr RPAREN stmt (ELSE stmt)?
    | WHILE LPAREN expr RPAREN stmt
    | BREAK SEMI
    | CONTINUE SEMI
    | RETURN expr? SEMI
    ;

expr
    : lOrExpr
    ;

lOrExpr
    : lAndExpr (OR lAndExpr)*
    ;

lAndExpr
    : relExpr (AND relExpr)*
    ;

relExpr
    : addExpr ((LT | GT | LE | GE | EQ | NE) addExpr)*
    ;

addExpr
    : mulExpr ((PLUS | MINUS) mulExpr)*
    ;

mulExpr
    : unaryExpr ((STAR | SLASH | PERCENT) unaryExpr)*
    ;

unaryExpr
    : primaryExpr
    | (PLUS | MINUS | NOT) unaryExpr
    ;

primaryExpr
    : ID LPAREN (expr (COMMA expr)*)? RPAREN
    | ID
    | NUMBER
    | LPAREN expr RPAREN
    ;

CONST: 'const';
INT: 'int';
VOID: 'void';
IF: 'if';
ELSE: 'else';
WHILE: 'while';
BREAK: 'break';
CONTINUE: 'continue';
RETURN: 'return';

OR: '||';
AND: '&&';
LE: '<=';
GE: '>=';
EQ: '==';
NE: '!=';
LT: '<';
GT: '>';
ASSIGN: '=';
PLUS: '+';
MINUS: '-';
STAR: '*';
SLASH: '/';
PERCENT: '%';
NOT: '!';
LPAREN: '(';
RPAREN: ')';
LBRACE: '{';
RBRACE: '}';
COMMA: ',';
SEMI: ';';

ID: [_A-Za-z] [_A-Za-z0-9]*;
// NUMBER matches only non-negative integers. Negative sign '-' is handled
// as a unary operator in the parser (unaryExpr rule), which is the standard
// compiler design approach. Including '-' in the lexer rule would cause
// lexical ambiguity with the binary subtraction operator (e.g., "1-5"
// would be incorrectly tokenized as NUMBER(1) NUMBER(-5) instead of
// NUMBER(1) MINUS NUMBER(5)).
NUMBER: '0' | [1-9] [0-9]*;

LINE_COMMENT: '//' ~[\r\n]* -> skip;
BLOCK_COMMENT: '/*' .*? '*/' -> skip;
WS: [ \t\r\n]+ -> skip;
