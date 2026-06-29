package toyc.semantic;

import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.OptionalInt;
import toyc.frontend.ast.ASTNode;
import toyc.frontend.ast.ASTVisitor;
import toyc.frontend.ast.Decl;
import toyc.frontend.ast.Expr;
import toyc.frontend.ast.FuncDef;
import toyc.frontend.ast.Program;
import toyc.frontend.ast.Stmt;

public final class SemanticAnalyzer implements ASTVisitor<FuncDef.Type, SemanticAnalyzer.Context> {
    private final Scope globals = new Scope(null);
    private final Map<ASTNode, Scope> scopeOf = new IdentityHashMap<>();
    private final Map<Expr, FuncDef.Type> typeOf = new IdentityHashMap<>();
    private final Map<Expr, Integer> constValueOf = new IdentityHashMap<>();
    private final Map<Expr, String> constErrorOf = new IdentityHashMap<>();
    private final Map<Decl, Symbol> symbolOfDecl = new IdentityHashMap<>();
    private final Map<FuncDef.Param, Symbol> symbolOfParam = new IdentityHashMap<>();
    private final Map<Stmt.Assign, Symbol> symbolOfAssign = new IdentityHashMap<>();
    private final Map<Expr.Id, Symbol> symbolOfUse = new IdentityHashMap<>();
    private final Map<Expr.FuncCall, FunctionSymbol> functionOfCall = new IdentityHashMap<>();
    private FunctionSymbol currentFunction;

    private SemanticAnalyzer() {
    }

    public static SemanticResult analyze(Program program) {
        SemanticAnalyzer analyzer = new SemanticAnalyzer();
        analyzer.visitProgram(program, new Context(analyzer.globals, 0));
        return new SemanticResult(analyzer.globals, analyzer.scopeOf, analyzer.typeOf,
                analyzer.constValueOf, analyzer.symbolOfDecl, analyzer.symbolOfParam,
                analyzer.symbolOfAssign, analyzer.symbolOfUse, analyzer.functionOfCall);
    }

    public static OptionalInt evalConst(Expr expr, SemanticResult result) {
        return result.constValueOf(expr);
    }

    @Override
    public FuncDef.Type visitProgram(Program node, Context context) {
        scopeOf.put(node, globals);
        for (ASTNode def : node.defs()) {
            if (def instanceof Decl decl) {
                decl.accept(this, context);
            } else if (def instanceof FuncDef funcDef) {
                defineFunctionHeader(funcDef);
                visitFunctionBody(funcDef);
            } else {
                throw error("unsupported top-level node: " + def.getClass().getSimpleName());
            }
        }

        FunctionSymbol main = globals.resolveFunction("main");
        if (main == null || main.returnType() != FuncDef.Type.INT || !main.params().isEmpty()) {
            throw error("program must define int main()");
        }
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitFuncDef(FuncDef node, Context context) {
        defineFunctionHeader(node);
        visitFunctionBody(node);
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitFuncDefParam(FuncDef.Param node, Context context) {
        return FuncDef.Type.INT;
    }

    @Override
    public FuncDef.Type visitVarDecl(Decl.VarDecl node, Context context) {
        scopeOf.put(node, context.scope());
        requireIntValue(node.init(), context, "variable initializer");
        if (context.scope().parent() == null && !constValueOf.containsKey(node.init())) {
            String constError = constErrorOf.get(node.init());
            if (constError != null) {
                throw error("global variable initializer " + constError + ": " + node.id());
            }
            throw error("global variable initializer must be compile-time constant: " + node.id());
        }
        Symbol symbol = new Symbol(node.id(), FuncDef.Type.INT,
                context.scope().parent() == null ? SymbolKind.GLOBAL_VAR : SymbolKind.LOCAL_VAR);
        defineSymbol(context.scope(), symbol, "duplicate declaration: " + node.id());
        symbolOfDecl.put(node, symbol);
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitConstDecl(Decl.ConstDecl node, Context context) {
        scopeOf.put(node, context.scope());
        requireIntValue(node.init(), context, "const initializer");
        Integer value = constValueOf.get(node.init());
        if (value == null) {
            String constError = constErrorOf.get(node.init());
            if (constError != null) {
                throw error("const initializer " + constError + ": " + node.id());
            }
            throw error("const initializer must be compile-time constant: " + node.id());
        }
        Symbol symbol = new Symbol(node.id(), FuncDef.Type.INT,
                context.scope().parent() == null ? SymbolKind.GLOBAL_CONST : SymbolKind.LOCAL_CONST,
                value, -1);
        defineSymbol(context.scope(), symbol, "duplicate declaration: " + node.id());
        symbolOfDecl.put(node, symbol);
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitBlockStmt(Stmt.Block node, Context context) {
        Scope blockScope = new Scope(context.scope());
        return visitBlockInScope(node, new Context(blockScope, context.loopDepth()));
    }

    @Override
    public FuncDef.Type visitEmptyStmt(Stmt.Empty node, Context context) {
        scopeOf.put(node, context.scope());
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitExprStmt(Stmt.ExprStmt node, Context context) {
        scopeOf.put(node, context.scope());
        node.expr().accept(this, context);
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitAssignStmt(Stmt.Assign node, Context context) {
        scopeOf.put(node, context.scope());
        Symbol target = context.scope().resolve(node.id());
        if (target == null) {
            throw error("assignment to undeclared identifier: " + node.id());
        }
        if (target.isConst()) {
            throw error("cannot assign to const: " + node.id());
        }
        symbolOfAssign.put(node, target);
        requireIntValue(node.expr(), context, "assignment right-hand side");
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitIfStmt(Stmt.If node, Context context) {
        scopeOf.put(node, context.scope());
        requireIntValue(node.cond(), context, "if condition");
        node.thenStmt().accept(this, context);
        if (node.elseStmt() != null) {
            node.elseStmt().accept(this, context);
        }
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitWhileStmt(Stmt.While node, Context context) {
        scopeOf.put(node, context.scope());
        requireIntValue(node.cond(), context, "while condition");
        node.body().accept(this, new Context(context.scope(), context.loopDepth() + 1));
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitBreakStmt(Stmt.Break node, Context context) {
        scopeOf.put(node, context.scope());
        if (context.loopDepth() == 0) {
            throw error("break outside loop");
        }
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitContinueStmt(Stmt.Continue node, Context context) {
        scopeOf.put(node, context.scope());
        if (context.loopDepth() == 0) {
            throw error("continue outside loop");
        }
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitReturnStmt(Stmt.Return node, Context context) {
        scopeOf.put(node, context.scope());
        if (currentFunction == null) {
            throw error("return outside function");
        }
        if (currentFunction.returnType() == FuncDef.Type.VOID) {
            if (node.expr() != null) {
                throw error("void function cannot return a value: " + currentFunction.name());
            }
        } else {
            if (node.expr() == null) {
                throw error("int function must return a value: " + currentFunction.name());
            }
            requireIntValue(node.expr(), context, "return value");
        }
        return FuncDef.Type.VOID;
    }

    @Override
    public FuncDef.Type visitBinaryExpr(Expr.Binary node, Context context) {
        FuncDef.Type left = node.left().accept(this, context);
        FuncDef.Type right = node.right().accept(this, context);
        if (left == FuncDef.Type.VOID || right == FuncDef.Type.VOID) {
            throw error("void expression cannot be used in binary operation");
        }
        evalBinaryConst(node);
        typeOf.put(node, FuncDef.Type.INT);
        return FuncDef.Type.INT;
    }

    @Override
    public FuncDef.Type visitUnaryExpr(Expr.Unary node, Context context) {
        FuncDef.Type type = node.expr().accept(this, context);
        if (type == FuncDef.Type.VOID) {
            throw error("void expression cannot be used in unary operation");
        }
        Integer value = constValueOf.get(node.expr());
        if (value != null) {
            int result = switch (node.op()) {
                case PLUS -> value;
                case MINUS -> -value;
                case NOT -> value == 0 ? 1 : 0;
            };
            constValueOf.put(node, result);
        }
        typeOf.put(node, FuncDef.Type.INT);
        return FuncDef.Type.INT;
    }

    @Override
    public FuncDef.Type visitIdExpr(Expr.Id node, Context context) {
        Symbol symbol = context.scope().resolve(node.name());
        if (symbol == null) {
            if (context.scope().resolveFunction(node.name()) != null) {
                throw error("function cannot be used as a value: " + node.name());
            }
            throw error("undeclared identifier: " + node.name());
        }
        symbolOfUse.put(node, symbol);
        if (symbol.constValue() != null) {
            constValueOf.put(node, symbol.constValue());
        }
        typeOf.put(node, symbol.type());
        return symbol.type();
    }

    @Override
    public FuncDef.Type visitIntLiteralExpr(Expr.IntLiteral node, Context context) {
        constValueOf.put(node, node.value());
        typeOf.put(node, FuncDef.Type.INT);
        return FuncDef.Type.INT;
    }

    @Override
    public FuncDef.Type visitFuncCallExpr(Expr.FuncCall node, Context context) {
        if (context.scope().resolve(node.id()) != null) {
            throw error("variable cannot be called as function: " + node.id());
        }
        FunctionSymbol function = context.scope().resolveFunction(node.id());
        if (function == null) {
            throw error("call to undefined function: " + node.id());
        }
        if (node.args().size() != function.params().size()) {
            throw error("function argument count mismatch: " + node.id());
        }
        for (Expr arg : node.args()) {
            requireIntValue(arg, context, "function argument");
        }
        functionOfCall.put(node, function);
        typeOf.put(node, function.returnType());
        return function.returnType();
    }

    private void defineFunctionHeader(FuncDef node) {
        List<Symbol> params = new ArrayList<>();
        for (int i = 0; i < node.params().size(); i++) {
            FuncDef.Param param = node.params().get(i);
            params.add(new Symbol(param.id(), FuncDef.Type.INT, SymbolKind.PARAM, null, i));
        }
        FunctionSymbol function = new FunctionSymbol(node.id(), node.returnType(), params, node);
        if (!globals.defineFunction(function)) {
            throw error("duplicate top-level declaration: " + node.id());
        }
    }

    private void visitFunctionBody(FuncDef node) {
        FunctionSymbol previous = currentFunction;
        currentFunction = globals.resolveFunction(node.id());
        Scope functionScope = new Scope(globals);
        scopeOf.put(node, functionScope);
        for (int i = 0; i < node.params().size(); i++) {
            FuncDef.Param param = node.params().get(i);
            Symbol symbol = currentFunction.params().get(i);
            defineSymbol(functionScope, symbol, "duplicate parameter: " + param.id());
            symbolOfParam.put(param, symbol);
            scopeOf.put(param, functionScope);
        }

        node.body().accept(this, new Context(functionScope, 0));
        if (node.returnType() == FuncDef.Type.INT && !mustReturn(node.body())) {
            throw error("int function may exit without return: " + node.id());
        }
        currentFunction = previous;
    }

    private FuncDef.Type visitBlockInScope(Stmt.Block block, Context context) {
        scopeOf.put(block, context.scope());
        for (Stmt stmt : block.stmts()) {
            stmt.accept(this, context);
        }
        return FuncDef.Type.VOID;
    }

    private void defineSymbol(Scope scope, Symbol symbol, String message) {
        if (!scope.define(symbol)) {
            throw error(message);
        }
    }

    private void requireIntValue(Expr expr, Context context, String place) {
        FuncDef.Type type = expr.accept(this, context);
        if (type != FuncDef.Type.INT) {
            throw error(place + " requires int value");
        }
    }

    private boolean mustReturn(Stmt stmt) {
        if (stmt instanceof Stmt.Return) {
            return true;
        }
        if (stmt instanceof Stmt.Block block) {
            for (Stmt child : block.stmts()) {
                if (mustReturn(child)) {
                    return true;
                }
            }
            return false;
        }
        if (stmt instanceof Stmt.If ifStmt) {
            return ifStmt.elseStmt() != null
                    && mustReturn(ifStmt.thenStmt())
                    && mustReturn(ifStmt.elseStmt());
        }
        // while 即使条件为常量真，也保守地不视为必定返回。
        return false;
    }

    private void evalBinaryConst(Expr.Binary node) {
        Integer left = constValueOf.get(node.left());
        Integer right = constValueOf.get(node.right());
        if (node.op() == Expr.BinaryOp.AND) {
            if (left == null) {
                propagateConstError(node, node.left());
                return;
            }
            if (left != null && left == 0) {
                constValueOf.put(node, 0);
                return;
            }
            if (right != null) {
                constValueOf.put(node, right != 0 ? 1 : 0);
            } else {
                propagateConstError(node, node.right());
            }
            return;
        }
        if (node.op() == Expr.BinaryOp.OR) {
            if (left == null) {
                propagateConstError(node, node.left());
                return;
            }
            if (left != null && left != 0) {
                constValueOf.put(node, 1);
                return;
            }
            if (right != null) {
                constValueOf.put(node, right != 0 ? 1 : 0);
            } else {
                propagateConstError(node, node.right());
            }
            return;
        }
        if (left == null || right == null) {
            propagateConstError(node, node.left());
            propagateConstError(node, node.right());
            return;
        }
        if (node.op() == Expr.BinaryOp.DIV && right == 0) {
            constErrorOf.put(node, "has division by zero");
            return;
        }
        if (node.op() == Expr.BinaryOp.MOD && right == 0) {
            constErrorOf.put(node, "has modulo by zero");
            return;
        }
        Integer value = switch (node.op()) {
            case ADD -> left + right;
            case SUB -> left - right;
            case MUL -> left * right;
            case DIV -> left / right;
            case MOD -> left % right;
            case LT -> left < right ? 1 : 0;
            case GT -> left > right ? 1 : 0;
            case LE -> left <= right ? 1 : 0;
            case GE -> left >= right ? 1 : 0;
            case EQ -> left.equals(right) ? 1 : 0;
            case NEQ -> !left.equals(right) ? 1 : 0;
            case AND, OR -> throw new IllegalStateException("short-circuit const operator should be handled earlier");
        };
        constValueOf.put(node, value);
    }

    private void propagateConstError(Expr target, Expr source) {
        String message = constErrorOf.get(source);
        if (message != null) {
            constErrorOf.put(target, message);
        }
    }

    private SemanticException error(String message) {
        return new SemanticException("semantic error: " + message);
    }

    record Context(Scope scope, int loopDepth) {
    }
}
