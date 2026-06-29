package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.GlobalVar;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

public final class GlobalAddr extends Instruction {
    private final Temp result;
    private final GlobalVar global;

    public GlobalAddr(Temp result, GlobalVar global) {
        this.result = result;
        this.global = global;
    }

    @Override
    public Temp result() {
        return result;
    }

    public GlobalVar global() {
        return global;
    }

    @Override
    public List<IRValue> operands() {
        return List.of(global);
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitGlobalAddr(this, context);
    }
}
