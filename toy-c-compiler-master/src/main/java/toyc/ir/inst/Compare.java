package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

public final class Compare extends Instruction {
    public enum Predicate {
        LT, GT, LE, GE, EQ, NE
    }

    private final Temp result;
    private final Predicate predicate;
    private final IRValue left;
    private final IRValue right;

    public Compare(Temp result, Predicate predicate, IRValue left, IRValue right) {
        this.result = result;
        this.predicate = predicate;
        this.left = left;
        this.right = right;
    }

    @Override
    public Temp result() {
        return result;
    }

    public Predicate predicate() {
        return predicate;
    }

    public IRValue left() {
        return left;
    }

    public IRValue right() {
        return right;
    }

    @Override
    public List<IRValue> operands() {
        return List.of(left, right);
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitCompare(this, context);
    }
}
