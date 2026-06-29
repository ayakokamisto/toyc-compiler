package toyc.ir.value;

import toyc.common.Type;
import toyc.ir.IRVisitor;

public final class LocalVar extends IRValue {
    private final String sourceName;
    private final int index;
    private final boolean parameter;

    public LocalVar(String sourceName, int index, boolean parameter) {
        super(Type.INT, "%" + sourceName + "." + index);
        this.sourceName = sourceName;
        this.index = index;
        this.parameter = parameter;
    }

    public String sourceName() {
        return sourceName;
    }

    public int index() {
        return index;
    }

    public boolean isParameter() {
        return parameter;
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitLocalVar(this, context);
    }
}
