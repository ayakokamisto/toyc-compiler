package toyc.ir.value;

import toyc.common.Type;
import toyc.ir.IRVisitor;

public final class Constant extends IRValue {
    private final int value;
    private final boolean named;

    private Constant(String name, int value, boolean named) {
        super(Type.INT, name);
        this.value = value;
        this.named = named;
    }

    public static Constant of(int value) {
        return new Constant(Integer.toString(value), value, false);
    }

    public static Constant named(String name, int value) {
        return new Constant("@" + name, value, true);
    }

    public int value() {
        return value;
    }

    public boolean isNamed() {
        return named;
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitConstant(this, context);
    }
}
