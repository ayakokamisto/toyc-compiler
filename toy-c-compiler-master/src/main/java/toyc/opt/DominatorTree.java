package toyc.opt;

import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;

final class DominatorTree {
    private final Function function;
    private final Map<BasicBlock, Set<BasicBlock>> dominators;
    private final Map<BasicBlock, List<BasicBlock>> children;

    private DominatorTree(
            Function function,
            Map<BasicBlock, Set<BasicBlock>> dominators,
            Map<BasicBlock, List<BasicBlock>> children) {
        this.function = function;
        this.dominators = dominators;
        this.children = children;
    }

    static DominatorTree build(Function function, ControlFlowGraph cfg) {
        List<BasicBlock> blocks = function.blocks();
        Map<BasicBlock, Set<BasicBlock>> dom = new IdentityHashMap<>();
        Set<BasicBlock> all = identitySet();
        all.addAll(blocks);

        for (BasicBlock block : blocks) {
            Set<BasicBlock> initial = identitySet();
            if (block == function.entryBlock()) {
                initial.add(block);
            } else {
                initial.addAll(all);
            }
            dom.put(block, initial);
        }

        boolean changed;
        do {
            changed = false;
            for (BasicBlock block : blocks) {
                if (block == function.entryBlock()) {
                    continue;
                }
                Set<BasicBlock> next = identitySet();
                next.addAll(all);
                for (BasicBlock pred : cfg.predecessors(block)) {
                    next.retainAll(dom.get(pred));
                }
                next.add(block);
                if (!sameSet(next, dom.get(block))) {
                    dom.put(block, next);
                    changed = true;
                }
            }
        } while (changed);

        Map<BasicBlock, List<BasicBlock>> children = new IdentityHashMap<>();
        for (BasicBlock block : blocks) {
            children.put(block, new ArrayList<>());
        }
        for (BasicBlock block : blocks) {
            if (block == function.entryBlock()) {
                continue;
            }
            BasicBlock best = null;
            for (BasicBlock candidate : dom.get(block)) {
                if (candidate == block) {
                    continue;
                }
                boolean dominatedByAllOtherStrictDominators = true;
                for (BasicBlock other : dom.get(block)) {
                    if (other != block && other != candidate && !dom.get(candidate).contains(other)) {
                        dominatedByAllOtherStrictDominators = false;
                        break;
                    }
                }
                if (dominatedByAllOtherStrictDominators) {
                    best = candidate;
                    break;
                }
            }
            if (best != null) {
                children.get(best).add(block);
            }
        }
        return new DominatorTree(function, dom, children);
    }

    List<BasicBlock> children(BasicBlock block) {
        return children.getOrDefault(block, List.of());
    }

    List<BasicBlock> dominanceFrontier(BasicBlock block, ControlFlowGraph cfg) {
        Set<BasicBlock> frontier = identitySet();
        for (BasicBlock candidate : function.blocks()) {
            for (BasicBlock pred : cfg.predecessors(candidate)) {
                if (dominates(block, pred) && !strictlyDominates(block, candidate)) {
                    frontier.add(candidate);
                    break;
                }
            }
        }
        return new ArrayList<>(frontier);
    }

    boolean dominates(BasicBlock dominator, BasicBlock block) {
        return dominators.getOrDefault(block, Set.of()).contains(dominator);
    }

    boolean strictlyDominates(BasicBlock dominator, BasicBlock block) {
        return dominator != block && dominates(dominator, block);
    }

    private static Set<BasicBlock> identitySet() {
        return Collections.newSetFromMap(new IdentityHashMap<>());
    }

    private static boolean sameSet(Set<BasicBlock> left, Set<BasicBlock> right) {
        return left.size() == right.size() && left.containsAll(right);
    }
}
