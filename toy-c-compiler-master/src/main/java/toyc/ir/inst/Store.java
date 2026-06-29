package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;

public final class Store extends Instruction {
    private final IRValue value;
    private final IRValue address;

    public Store(IRValue value, IRValue address) {
        this.value = value;
        this.address = address;
    }

    public IRValue value() {
        return value;
    }

    public IRValue address() {
        return address;
    }

    @Override
    public List<IRValue> operands() {
        return List.of(value, address);
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitStore(this, context);
    }
}
