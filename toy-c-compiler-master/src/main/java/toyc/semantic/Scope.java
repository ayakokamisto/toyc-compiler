package toyc.semantic;

import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.Map;

public final class Scope {
    private final Scope parent;
    private final Map<String, Symbol> symbols = new LinkedHashMap<>();
    private final Map<String, FunctionSymbol> functions = new LinkedHashMap<>();

    public Scope(Scope parent) {
        this.parent = parent;
    }

    public Scope parent() {
        return parent;
    }

    public boolean define(Symbol symbol) {
        if (symbols.containsKey(symbol.name()) || functions.containsKey(symbol.name())) {
            return false;
        }
        symbols.put(symbol.name(), symbol);
        return true;
    }

    public boolean defineFunction(FunctionSymbol function) {
        if (symbols.containsKey(function.name()) || functions.containsKey(function.name())) {
            return false;
        }
        functions.put(function.name(), function);
        return true;
    }

    public Symbol resolve(String name) {
        Symbol symbol = symbols.get(name);
        if (symbol != null) {
            return symbol;
        }
        return parent == null ? null : parent.resolve(name);
    }

    public FunctionSymbol resolveFunction(String name) {
        FunctionSymbol function = functions.get(name);
        if (function != null) {
            return function;
        }
        return parent == null ? null : parent.resolveFunction(name);
    }

    public Collection<Symbol> symbols() {
        return symbols.values();
    }

    public Collection<FunctionSymbol> functions() {
        return functions.values();
    }
}
