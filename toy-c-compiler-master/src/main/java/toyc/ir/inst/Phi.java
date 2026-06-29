package toyc.ir.inst;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.value.IRValue;
import toyc.ir.value.Label;
import toyc.ir.value.Temp;

public final class Phi extends Instruction {
    private final Temp result;
    private final List<Incoming> incoming = new ArrayList<>();

    public Phi(Temp result) {
        this.result = result;
    }

    @Override
    public Temp result() {
        return result;
    }

    public List<Incoming> incoming() {
        return Collections.unmodifiableList(incoming);
    }

    public void addIncoming(Label predecessor, IRValue value) {
        incoming.add(new Incoming(predecessor, value));
    }

    @Override
    public List<IRValue> operands() {
        return incoming.stream().map(Incoming::value).toList();
    }

    @Override
    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitPhi(this, context);
    }

    public record Incoming(Label predecessor, IRValue value) {
    }
}
