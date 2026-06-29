package toyc.ir.block;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import toyc.ir.IRVisitor;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Phi;
import toyc.ir.value.Label;

public final class BasicBlock {
    private final Label label;
    private final List<Phi> phis = new ArrayList<>();
    private final List<Instruction> instructions = new ArrayList<>();
    private Instruction terminator;

    public BasicBlock(Label label) {
        this.label = label;
    }

    public Label label() {
        return label;
    }

    public List<Phi> phis() {
        return Collections.unmodifiableList(phis);
    }

    public List<Instruction> instructions() {
        return Collections.unmodifiableList(instructions);
    }

    public List<Instruction> allInstructions() {
        List<Instruction> all = new ArrayList<>(phis.size() + instructions.size() + (terminator == null ? 0 : 1));
        all.addAll(phis);
        all.addAll(instructions);
        if (terminator != null) {
            all.add(terminator);
        }
        return Collections.unmodifiableList(all);
    }

    public void addPhi(Phi phi) {
        phis.add(phi);
    }

    public void clearPhis() {
        phis.clear();
    }

    public void addInstruction(Instruction instruction) {
        if (isTerminated()) {
            throw new IllegalStateException("cannot append instruction after terminator in " + label);
        }
        if (instruction instanceof Phi phi) {
            addPhi(phi);
        } else if (instruction.isTerminator()) {
            terminator = instruction;
        } else {
            instructions.add(instruction);
        }
    }

    public void replaceInstructions(List<Instruction> newInstructions) {
        phis.clear();
        instructions.clear();
        terminator = null;
        for (Instruction instruction : newInstructions) {
            addInstruction(instruction);
        }
    }

    public void replaceBody(List<Instruction> newInstructions) {
        for (Instruction instruction : newInstructions) {
            if (instruction instanceof Phi || instruction.isTerminator()) {
                throw new IllegalArgumentException("block body cannot contain phi or terminator");
            }
        }
        instructions.clear();
        instructions.addAll(newInstructions);
    }

    public void setTerminator(Instruction terminator) {
        if (terminator != null && !terminator.isTerminator()) {
            throw new IllegalArgumentException("terminator instruction expected");
        }
        this.terminator = terminator;
    }

    public boolean isTerminated() {
        return terminator != null;
    }

    public Instruction terminator() {
        return terminator;
    }

    public boolean hasTerminatorType(Class<? extends Instruction> type) {
        return terminator != null && type.isInstance(terminator);
    }

    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitBasicBlock(this, context);
    }
}
