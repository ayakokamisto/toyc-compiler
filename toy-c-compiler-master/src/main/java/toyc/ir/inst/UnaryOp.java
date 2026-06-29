package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

public final class UnaryOp extends Instruction {
    public enum Op {
        NEG, NOT
    }

    private final Temp result;
    private final Op op;
    private final IRValue value;

    public UnaryOp(Temp result, Op op, IRValue value) {
        this.result = result;
        this.op = op;
        this.value = value;
    }

    @Override
    public Temp result() {
        return result;
    }

    public Op op() {
        return op;
    }

    public IRValue value() {
        return value;
    }

    @Override
    public List<IRValue> operands() {
        return List.of(value);
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitUnaryOp(this, context);
    }
}
