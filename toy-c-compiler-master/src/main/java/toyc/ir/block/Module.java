package toyc.ir.block;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.Constant;
import toyc.ir.value.GlobalVar;

public final class Module {
    private final List<GlobalVar> globals = new ArrayList<>();
    private final List<Constant> globalConstants = new ArrayList<>();
    private final List<Function> functions = new ArrayList<>();
    private Function mainFunction;

    public List<GlobalVar> globals() {
        return Collections.unmodifiableList(globals);
    }

    public List<Constant> globalConstants() {
        return Collections.unmodifiableList(globalConstants);
    }

    public List<Function> functions() {
        return Collections.unmodifiableList(functions);
    }

    public Function mainFunction() {
        return mainFunction;
    }

    public void addGlobal(GlobalVar global) {
        globals.add(global);
    }

    public void addGlobalConstant(Constant constant) {
        globalConstants.add(constant);
    }

    public void addFunction(Function function) {
        functions.add(function);
        if ("main".equals(function.name())) {
            mainFunction = function;
        }
    }

    public void replaceFunctions(List<Function> newFunctions) {
        functions.clear();
        mainFunction = null;
        for (Function function : newFunctions) {
            addFunction(function);
        }
    }

    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitModule(this, context);
    }
}
