package toyc.ir.value;

import toyc.common.Type;
import toyc.ir.IRVisitor;

public final class GlobalVar extends IRValue {
    private final int initialValue;

    public GlobalVar(String name, int initialValue) {
        super(Type.INT, "@" + name);
        this.initialValue = initialValue;
    }

    public int initialValue() {
        return initialValue;
    }

    public String symbolName() {
        return name().substring(1);
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitGlobalVar(this, context);
    }
}
