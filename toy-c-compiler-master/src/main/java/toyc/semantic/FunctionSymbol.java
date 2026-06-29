package toyc.semantic;

import java.util.List;
import toyc.frontend.ast.FuncDef;

public final class FunctionSymbol {
    private final String name;
    private final FuncDef.Type returnType;
    private final List<Symbol> params;
    private final FuncDef declaration;

    public FunctionSymbol(String name, FuncDef.Type returnType, List<Symbol> params, FuncDef declaration) {
        this.name = name;
        this.returnType = returnType;
        this.params = List.copyOf(params);
        this.declaration = declaration;
    }

    public String name() {
        return name;
    }

    public FuncDef.Type returnType() {
        return returnType;
    }

    public List<Symbol> params() {
        return params;
    }

    public FuncDef declaration() {
        return declaration;
    }
}
