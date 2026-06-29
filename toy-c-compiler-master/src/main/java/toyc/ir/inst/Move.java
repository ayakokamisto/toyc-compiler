package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

public final class Move extends Instruction {
    private final Temp result;
    private final IRValue value;

    public Move(Temp result, IRValue value) {
        this.result = result;
        this.value = value;
    }

    @Override
    public Temp result() {
        return result;
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
        return visitor.visitMove(this, context);
    }
}
