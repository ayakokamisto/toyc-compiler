package toyc.opt;

import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.Branch;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.Instruction;
import toyc.ir.value.Constant;
import toyc.ir.value.Label;

final class ControlFlowSimplifier {
    private ControlFlowSimplifier() {
    }

    static boolean simplify(Function function) {
        boolean changed = false;
        boolean iterationChanged;
        do {
            iterationChanged = false;
            iterationChanged |= removeInstructionsAfterTerminator(function);
            iterationChanged |= foldConstantBranches(function);
            iterationChanged |= bypassJumpOnlyBlocks(function);
            iterationChanged |= mergeLinearBlocks(function);
            iterationChanged |= removeUnreachableBlocks(function);
            changed |= iterationChanged;
        } while (iterationChanged);
        return changed;
    }

    private static boolean removeInstructionsAfterTerminator(Function function) {
        return false;
    }

    private static boolean foldConstantBranches(Function function) {
        boolean changed = false;
        for (BasicBlock block : function.blocks()) {
            Instruction terminator = block.terminator();
            if (terminator instanceof CondBranch branch && branch.condition() instanceof Constant constant) {
                replaceTerminator(block, new Branch(constant.value() != 0 ? branch.trueTarget() : branch.falseTarget()));
                changed = true;
            }
        }
        return changed;
    }

    private static boolean bypassJumpOnlyBlocks(Function function) {
        Map<Label, Label> replacements = new IdentityHashMap<>();
        for (BasicBlock block : function.blocks()) {
            if (block == function.entryBlock() || !block.phis().isEmpty() || !block.instructions().isEmpty()) {
                continue;
            }
            if (block.terminator() instanceof Branch branch && branch.target() != block.label()) {
                replacements.put(block.label(), resolveJumpTarget(branch.target(), replacements));
            }
        }

        boolean changed = false;
        for (BasicBlock block : function.blocks()) {
            Instruction terminator = block.terminator();
            Instruction rewritten = rewriteTargets(terminator, replacements);
            if (rewritten != terminator) {
                replaceTerminator(block, rewritten);
                changed = true;
            }
        }
        return changed;
    }

    private static boolean mergeLinearBlocks(Function function) {
        ControlFlowGraph cfg = ControlFlowGraph.build(function);
        boolean changed = false;
        for (BasicBlock block : new ArrayList<>(function.blocks())) {
            if (!(block.terminator() instanceof Branch)) {
                continue;
            }
            BasicBlock successor = singleSuccessor(cfg, block);
            if (successor == null || successor == block || cfg.predecessors(successor).size() != 1) {
                continue;
            }
            if (successor == function.entryBlock() || !successor.phis().isEmpty()) {
                continue;
            }
            List<Instruction> merged = new ArrayList<>(block.instructions());
            merged.addAll(successor.instructions());
            block.replaceBody(merged);
            block.setTerminator(successor.terminator());

            List<BasicBlock> blocks = new ArrayList<>(function.blocks());
            blocks.remove(successor);
            function.replaceBlocks(blocks);
            changed = true;
            break;
        }
        return changed;
    }

    private static boolean removeUnreachableBlocks(Function function) {
        ControlFlowGraph cfg = ControlFlowGraph.build(function);
        List<BasicBlock> reachable = new ArrayList<>();
        boolean changed = false;
        for (BasicBlock block : function.blocks()) {
            if (cfg.isReachable(block)) {
                reachable.add(block);
            } else {
                changed = true;
            }
        }
        if (changed) {
            function.replaceBlocks(reachable);
        }
        return changed;
    }

    private static BasicBlock singleSuccessor(ControlFlowGraph cfg, BasicBlock block) {
        List<BasicBlock> successors = cfg.successors(block);
        return successors.size() == 1 ? successors.get(0) : null;
    }

    private static Label resolveJumpTarget(Label label, Map<Label, Label> replacements) {
        Label current = label;
        while (true) {
            Label next = replacements.get(current);
            if (next == null || next == current) {
                return current;
            }
            current = next;
        }
    }

    private static Instruction rewriteTargets(Instruction instruction, Map<Label, Label> replacements) {
        if (instruction instanceof Branch branch) {
            Label target = resolveJumpTarget(branch.target(), replacements);
            return target == branch.target() ? instruction : new Branch(target);
        }
        if (instruction instanceof CondBranch branch) {
            Label trueTarget = resolveJumpTarget(branch.trueTarget(), replacements);
            Label falseTarget = resolveJumpTarget(branch.falseTarget(), replacements);
            if (trueTarget == falseTarget) {
                return new Branch(trueTarget);
            }
            if (trueTarget == branch.trueTarget() && falseTarget == branch.falseTarget()) {
                return instruction;
            }
            return new CondBranch(branch.condition(), trueTarget, falseTarget);
        }
        return instruction;
    }

    private static void replaceTerminator(BasicBlock block, Instruction terminator) {
        if (!block.isTerminated()) {
            throw new IllegalStateException("block does not end with a terminator: " + block.label().name());
        }
        block.setTerminator(terminator);
    }
}
