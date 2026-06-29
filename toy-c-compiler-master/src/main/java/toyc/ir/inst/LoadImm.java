package toyc.ir.inst;

import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.Constant;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

public final class LoadImm extends Instruction {
    private final Temp result;
    private final Constant constant;

    public LoadImm(Temp result, int value) {
        this(result, Constant.of(value));
    }

    public LoadImm(Temp result, Constant constant) {
        this.result = result;
        this.constant = constant;
    }

    @Override
    public Temp result() {
        return result;
    }

    public Constant constant() {
        return constant;
    }

    @Override
    public List<IRValue> operands() {
        return List.of(constant);
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitLoadImm(this, context);
    }
}
