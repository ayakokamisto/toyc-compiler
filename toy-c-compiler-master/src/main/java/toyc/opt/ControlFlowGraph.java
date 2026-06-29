package toyc.opt;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.Branch;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.Instruction;
import toyc.ir.value.Label;

final class ControlFlowGraph {
    private final Map<BasicBlock, List<BasicBlock>> successors;
    private final Map<BasicBlock, List<BasicBlock>> predecessors;
    private final Set<BasicBlock> reachableBlocks;

    private ControlFlowGraph(
            Map<BasicBlock, List<BasicBlock>> successors,
            Map<BasicBlock, List<BasicBlock>> predecessors,
            Set<BasicBlock> reachableBlocks) {
        this.successors = successors;
        this.predecessors = predecessors;
        this.reachableBlocks = reachableBlocks;
    }

    static ControlFlowGraph build(Function function) {
        Map<Label, BasicBlock> blockByLabel = new IdentityHashMap<>();
        Map<BasicBlock, List<BasicBlock>> successors = new IdentityHashMap<>();
        Map<BasicBlock, List<BasicBlock>> predecessors = new IdentityHashMap<>();

        for (BasicBlock block : function.blocks()) {
            blockByLabel.put(block.label(), block);
            successors.put(block, new ArrayList<>());
            predecessors.put(block, new ArrayList<>());
        }

        for (BasicBlock block : function.blocks()) {
            for (Label target : successorLabels(block)) {
                BasicBlock successor = blockByLabel.get(target);
                if (successor == null) {
                    throw new IllegalStateException("branch target does not exist: " + target.name());
                }
                addUnique(successors.get(block), successor);
                addUnique(predecessors.get(successor), block);
            }
        }

        return new ControlFlowGraph(successors, predecessors, findReachable(function.entryBlock(), successors));
    }

    List<BasicBlock> successors(BasicBlock block) {
        return successors.getOrDefault(block, List.of());
    }

    List<BasicBlock> predecessors(BasicBlock block) {
        return predecessors.getOrDefault(block, List.of());
    }

    boolean isReachable(BasicBlock block) {
        return reachableBlocks.contains(block);
    }

    Set<BasicBlock> reachableBlocks() {
        return Collections.unmodifiableSet(reachableBlocks);
    }

    private static List<Label> successorLabels(BasicBlock block) {
        Instruction terminator = block.terminator();
        if (terminator instanceof Branch branch) {
            return List.of(branch.target());
        }
        if (terminator instanceof CondBranch branch) {
            return List.of(branch.trueTarget(), branch.falseTarget());
        }
        return List.of();
    }

    private static Set<BasicBlock> findReachable(
            BasicBlock entryBlock,
            Map<BasicBlock, List<BasicBlock>> successors) {
        Set<BasicBlock> reachable = Collections.newSetFromMap(new IdentityHashMap<>());
        ArrayDeque<BasicBlock> worklist = new ArrayDeque<>();
        worklist.add(entryBlock);
        while (!worklist.isEmpty()) {
            BasicBlock block = worklist.removeFirst();
            if (!reachable.add(block)) {
                continue;
            }
            for (BasicBlock successor : successors.getOrDefault(block, List.of())) {
                worklist.addLast(successor);
            }
        }
        return reachable;
    }

    private static void addUnique(List<BasicBlock> blocks, BasicBlock block) {
        for (BasicBlock existing : blocks) {
            if (existing == block) {
                return;
            }
        }
        blocks.add(block);
    }
}
