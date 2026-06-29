package toyc.ir.inst;

import java.util.List;
import toyc.common.Type;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

public final class Call extends Instruction {
    private final Temp result;
    private final String functionName;
    private final Type returnType;
    private final List<IRValue> args;

    public Call(Temp result, String functionName, Type returnType, List<IRValue> args) {
        this.result = result;
        this.functionName = functionName;
        this.returnType = returnType;
        this.args = List.copyOf(args);
    }

    @Override
    public Temp result() {
        return result;
    }

    public String functionName() {
        return functionName;
    }

    public Type returnType() {
        return returnType;
    }

    public List<IRValue> args() {
        return args;
    }

    @Override
    public List<IRValue> operands() {
        return args;
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitCall(this, context);
    }
}
