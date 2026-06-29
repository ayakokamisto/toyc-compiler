package toyc.ir.value;

import toyc.common.Type;
import toyc.ir.IRVisitor;

public abstract class IRValue {
    private final Type type;
    private final String name;

    protected IRValue(Type type, String name) {
        this.type = type;
        this.name = name;
    }

    public Type type() {
        return type;
    }

    public String name() {
        return name;
    }

    @Override
    public String toString() {
        return name;
    }

    public abstract <R, C> R accept(IRVisitor<R, C> visitor, C context);
}
