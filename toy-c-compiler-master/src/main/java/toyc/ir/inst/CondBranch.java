package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;
import toyc.ir.value.Label;

public final class CondBranch extends Instruction {
    private final IRValue condition;
    private final Label trueTarget;
    private final Label falseTarget;

    public CondBranch(IRValue condition, Label trueTarget, Label falseTarget) {
        this.condition = condition;
        this.trueTarget = trueTarget;
        this.falseTarget = falseTarget;
    }

    public IRValue condition() {
        return condition;
    }

    public Label trueTarget() {
        return trueTarget;
    }

    public Label falseTarget() {
        return falseTarget;
    }

    @Override
    public List<IRValue> operands() {
        return List.of(condition);
    }

    @Override
    public boolean isTerminator() {
        return true;
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitCondBranch(this, context);
    }
}
