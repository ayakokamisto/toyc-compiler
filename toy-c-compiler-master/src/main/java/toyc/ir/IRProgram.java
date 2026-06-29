package toyc.ir;

import toyc.ir.block.Module;

public final class IRProgram {
    private final Module module;

    public IRProgram() {
        this(new Module());
    }

    public IRProgram(Module module) {
        this.module = module;
    }

    public Module module() {
        return module;
    }

    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitProgram(this, context);
    }
}
