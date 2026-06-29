package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

public final class BinaryOp extends Instruction {
    public enum Op {
        ADD, SUB, MUL, DIV, MOD
    }

    private final Temp result;
    private final Op op;
    private final IRValue left;
    private final IRValue right;

    public BinaryOp(Temp result, Op op, IRValue left, IRValue right) {
        this.result = result;
        this.op = op;
        this.left = left;
        this.right = right;
    }

    @Override
    public Temp result() {
        return result;
    }

    public Op op() {
        return op;
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
        return visitor.visitBinaryOp(this, context);
    }
}
