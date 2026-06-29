package toyc.ir.inst;

import toyc.ir.IRVisitor;
import toyc.ir.value.Label;

public final class Branch extends Instruction {
    private final Label target;

    public Branch(Label target) {
        this.target = target;
    }

    public Label target() {
        return target;
    }

    @Override
    public boolean isTerminator() {
        return true;
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitBranch(this, context);
    }
}
