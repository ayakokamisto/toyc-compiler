package toyc.ir.inst;

import toyc.ir.IRVisitor;
import toyc.ir.value.LocalVar;

public final class Alloca extends Instruction {
    private final LocalVar result;

    public Alloca(LocalVar result) {
        this.result = result;
    }

    @Override
    public LocalVar result() {
        return result;
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitAlloca(this, context);
    }
}
