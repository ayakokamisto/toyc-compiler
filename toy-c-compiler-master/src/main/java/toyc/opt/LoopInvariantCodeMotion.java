package toyc.opt;

import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Set;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.Branch;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.Compare;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.LoadImm;
import toyc.ir.inst.Move;
import toyc.ir.inst.UnaryOp;
import toyc.ir.value.IRValue;

final class LoopInvariantCodeMotion {
    private LoopInvariantCodeMotion() {
    }

    static boolean hoist(Function function) {
        ControlFlowGraph cfg = ControlFlowGraph.build(function);
        boolean changed = false;
        for (BasicBlock cond : function.blocks()) {
            Loop loop = findSimpleLoop(cond, cfg);
            if (loop != null) {
                changed |= hoistLoop(loop);
            }
        }
        return changed;
    }

    private static Loop findSimpleLoop(BasicBlock cond, ControlFlowGraph cfg) {
        if (!(cond.terminator() instanceof CondBranch branch)) {
            return null;
        }
        BasicBlock trueTarget = singleBlockWithLabel(cfg.successors(cond), branch.trueTarget());
        BasicBlock falseTarget = singleBlockWithLabel(cfg.successors(cond), branch.falseTarget());
        if (trueTarget == null || falseTarget == null) {
            return null;
        }

        BasicBlock body = null;
        if (branchesTo(trueTarget, cond)) {
            body = trueTarget;
        } else if (branchesTo(falseTarget, cond)) {
            body = falseTarget;
        }
        if (body == null || cfg.predecessors(cond).size() != 2) {
            return null;
        }

        BasicBlock preheader = null;
        for (BasicBlock predecessor : cfg.predecessors(cond)) {
            if (predecessor != body) {
                preheader = predecessor;
            }
        }
        if (preheader == null || !(preheader.terminator() instanceof Branch preBranch) || preBranch.target() != cond.label()) {
            return null;
        }
        if (!(body.terminator() instanceof Branch bodyBranch) || bodyBranch.target() != cond.label()) {
            return null;
        }
        return new Loop(preheader, cond, body);
    }

    private static boolean hoistLoop(Loop loop) {
        Set<IRValue> definedInLoop = Collections.newSetFromMap(new IdentityHashMap<>());
        for (Instruction instruction : loop.cond().instructions()) {
            if (instruction.result() != null) {
                definedInLoop.add(instruction.result());
            }
        }
        for (Instruction instruction : loop.body().instructions()) {
            if (instruction.result() != null) {
                definedInLoop.add(instruction.result());
            }
        }

        List<Instruction> remaining = new ArrayList<>(loop.body().instructions());
        List<Instruction> hoisted = new ArrayList<>();
        boolean moved;
        do {
            moved = false;
            List<Instruction> nextRemaining = new ArrayList<>();
            for (Instruction instruction : remaining) {
                if (isHoistable(instruction) && operandsOutsideLoop(instruction, definedInLoop)) {
                    hoisted.add(instruction);
                    if (instruction.result() != null) {
                        definedInLoop.remove(instruction.result());
                    }
                    moved = true;
                } else {
                    nextRemaining.add(instruction);
                }
            }
            remaining = nextRemaining;
        } while (moved);
        if (hoisted.isEmpty()) {
            return false;
        }

        List<Instruction> preheaderBody = new ArrayList<>(loop.preheader().instructions());
        preheaderBody.addAll(hoisted);
        loop.preheader().replaceBody(preheaderBody);
        loop.body().replaceBody(remaining);
        return true;
    }

    private static boolean isHoistable(Instruction instruction) {
        return instruction instanceof LoadImm
                || instruction instanceof GlobalAddr
                || instruction instanceof Move
                || instruction instanceof UnaryOp
                || instruction instanceof BinaryOp
                || instruction instanceof Compare;
    }

    private static boolean operandsOutsideLoop(Instruction instruction, Set<IRValue> definedInLoop) {
        for (IRValue operand : instruction.operands()) {
            if (definedInLoop.contains(operand)) {
                return false;
            }
        }
        return true;
    }

    private static BasicBlock singleBlockWithLabel(List<BasicBlock> blocks, toyc.ir.value.Label label) {
        for (BasicBlock block : blocks) {
            if (block.label() == label) {
                return block;
            }
        }
        return null;
    }

    private static boolean branchesTo(BasicBlock block, BasicBlock target) {
        return block.terminator() instanceof Branch branch && branch.target() == target.label();
    }

    private record Loop(BasicBlock preheader, BasicBlock cond, BasicBlock body) {
    }
}
