package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;

public abstract class Instruction {
    public IRValue result() {
        return null;
    }

    public List<IRValue> operands() {
        return List.of();
    }

    public boolean isTerminator() {
        return false;
    }

    public abstract <R, C> R accept(IRVisitor<R, C> visitor, C context);
}
