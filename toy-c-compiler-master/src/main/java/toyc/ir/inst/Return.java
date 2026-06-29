package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;

public final class Return extends Instruction {
    private final IRValue value;

    public Return(IRValue value) {
        this.value = value;
    }

    public IRValue value() {
        return value;
    }

    @Override
    public List<IRValue> operands() {
        return value == null ? List.of() : List.of(value);
    }

    @Override
    public boolean isTerminator() {
        return true;
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitReturn(this, context);
    }
}
