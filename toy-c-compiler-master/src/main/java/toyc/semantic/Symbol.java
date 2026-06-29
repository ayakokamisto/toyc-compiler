package toyc.semantic;

import toyc.frontend.ast.FuncDef;

public final class Symbol {
    private final String name;
    private final FuncDef.Type type;
    private final SymbolKind kind;
    private final Integer constValue;
    private final int paramIndex;

    public Symbol(String name, FuncDef.Type type, SymbolKind kind) {
        this(name, type, kind, null, -1);
    }

    public Symbol(String name, FuncDef.Type type, SymbolKind kind, Integer constValue, int paramIndex) {
        this.name = name;
        this.type = type;
        this.kind = kind;
        this.constValue = constValue;
        this.paramIndex = paramIndex;
    }

    public String name() {
        return name;
    }

    public FuncDef.Type type() {
        return type;
    }

    public SymbolKind kind() {
        return kind;
    }

    public boolean isConst() {
        return kind == SymbolKind.GLOBAL_CONST || kind == SymbolKind.LOCAL_CONST;
    }

    public boolean isGlobal() {
        return kind == SymbolKind.GLOBAL_VAR || kind == SymbolKind.GLOBAL_CONST;
    }

    public Integer constValue() {
        return constValue;
    }

    public int paramIndex() {
        return paramIndex;
    }
}
