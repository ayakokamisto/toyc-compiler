package toyc.semantic;

import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.Map;
import java.util.OptionalInt;
import toyc.frontend.ast.ASTNode;
import toyc.frontend.ast.Decl;
import toyc.frontend.ast.Expr;
import toyc.frontend.ast.FuncDef;
import toyc.frontend.ast.Stmt;

public final class SemanticResult {
    private final Scope globals;
    private final Map<ASTNode, Scope> scopeOf;
    private final Map<Expr, FuncDef.Type> typeOf;
    private final Map<Expr, Integer> constValueOf;
    private final Map<Decl, Symbol> symbolOfDecl;
    private final Map<FuncDef.Param, Symbol> symbolOfParam;
    private final Map<Stmt.Assign, Symbol> symbolOfAssign;
    private final Map<Expr.Id, Symbol> symbolOfUse;
    private final Map<Expr.FuncCall, FunctionSymbol> functionOfCall;

    SemanticResult(Scope globals,
            Map<ASTNode, Scope> scopeOf,
            Map<Expr, FuncDef.Type> typeOf,
            Map<Expr, Integer> constValueOf,
            Map<Decl, Symbol> symbolOfDecl,
            Map<FuncDef.Param, Symbol> symbolOfParam,
            Map<Stmt.Assign, Symbol> symbolOfAssign,
            Map<Expr.Id, Symbol> symbolOfUse,
            Map<Expr.FuncCall, FunctionSymbol> functionOfCall) {
        this.globals = globals;
        this.scopeOf = copy(scopeOf);
        this.typeOf = copy(typeOf);
        this.constValueOf = copy(constValueOf);
        this.symbolOfDecl = copy(symbolOfDecl);
        this.symbolOfParam = copy(symbolOfParam);
        this.symbolOfAssign = copy(symbolOfAssign);
        this.symbolOfUse = copy(symbolOfUse);
        this.functionOfCall = copy(functionOfCall);
    }

    public Scope globals() {
        return globals;
    }

    public Scope scopeOf(ASTNode node) {
        return scopeOf.get(node);
    }

    public FuncDef.Type typeOf(Expr expr) {
        return typeOf.get(expr);
    }

    public OptionalInt constValueOf(Expr expr) {
        Integer value = constValueOf.get(expr);
        return value == null ? OptionalInt.empty() : OptionalInt.of(value);
    }

    public Symbol symbolOf(Decl decl) {
        return symbolOfDecl.get(decl);
    }

    public Symbol symbolOf(FuncDef.Param param) {
        return symbolOfParam.get(param);
    }

    public Symbol symbolOf(Stmt.Assign assign) {
        return symbolOfAssign.get(assign);
    }

    public Symbol symbolOf(Expr.Id use) {
        return symbolOfUse.get(use);
    }

    public FunctionSymbol functionOf(Expr.FuncCall call) {
        return functionOfCall.get(call);
    }

    public Map<ASTNode, Scope> scopes() {
        return scopeOf;
    }

    public Map<Expr, FuncDef.Type> types() {
        return typeOf;
    }

    public Map<Expr, Integer> constValues() {
        return constValueOf;
    }

    private static <K, V> Map<K, V> copy(Map<K, V> source) {
        Map<K, V> target = new IdentityHashMap<>();
        target.putAll(source);
        return Collections.unmodifiableMap(target);
    }
}
