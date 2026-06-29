package toyc.ir.value;

import toyc.common.Type;
import toyc.ir.IRVisitor;

public final class Label extends IRValue {
    public Label(String name) {
        super(Type.VOID, name);
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitLabel(this, context);
    }
}
