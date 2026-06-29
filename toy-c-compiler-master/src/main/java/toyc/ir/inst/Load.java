package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

public final class Load extends Instruction {
    private final Temp result;
    private final IRValue address;

    public Load(Temp result, IRValue address) {
        this.result = result;
        this.address = address;
    }

    @Override
    public Temp result() {
        return result;
    }

    public IRValue address() {
        return address;
    }

    @Override
    public List<IRValue> operands() {
        return List.of(address);
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitLoad(this, context);
    }
}
