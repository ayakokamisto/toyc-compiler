package toyc.ir.value;

import toyc.common.Type;
import toyc.ir.IRVisitor;

public final class Temp extends IRValue {
    private final int index;

    public Temp(int index, Type type) {
        super(type, "%t" + index);
        this.index = index;
    }

    public int index() {
        return index;
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitTemp(this, context);
    }
}
