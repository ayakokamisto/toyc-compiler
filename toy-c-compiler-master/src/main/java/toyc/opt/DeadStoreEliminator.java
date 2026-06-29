package toyc.opt;

import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Store;
import toyc.ir.value.IRValue;
import toyc.ir.value.LocalVar;

final class DeadStoreEliminator {
    private DeadStoreEliminator() {
    }

    static boolean eliminate(Function function) {
        ControlFlowGraph cfg = ControlFlowGraph.build(function);
        Map<BasicBlock, BlockUseDef> useDefs = computeUseDefs(function);
        Map<BasicBlock, Set<LocalVar>> liveIn = new IdentityHashMap<>();
        Map<BasicBlock, Set<LocalVar>> liveOut = new IdentityHashMap<>();
        for (BasicBlock block : function.blocks()) {
            liveIn.put(block, identitySet());
            liveOut.put(block, identitySet());
        }

        boolean changed;
        do {
            changed = false;
            List<BasicBlock> blocks = function.blocks();
            for (int i = blocks.size() - 1; i >= 0; i--) {
                BasicBlock block = blocks.get(i);
                Set<LocalVar> out = identitySet();
                for (BasicBlock successor : cfg.successors(block)) {
                    out.addAll(liveIn.get(successor));
                }

                BlockUseDef useDef = useDefs.get(block);
                Set<LocalVar> in = identitySet();
                in.addAll(out);
                in.removeAll(useDef.defs());
                in.addAll(useDef.uses());

                if (!sameSet(out, liveOut.get(block))) {
                    liveOut.put(block, out);
                    changed = true;
                }
                if (!sameSet(in, liveIn.get(block))) {
                    liveIn.put(block, in);
                    changed = true;
                }
            }
        } while (changed);

        boolean removed = false;
        for (BasicBlock block : function.blocks()) {
            Set<LocalVar> live = identitySet();
            live.addAll(liveOut.get(block));
            List<Instruction> keptReversed = new ArrayList<>();

            List<Instruction> instructions = block.allInstructions();
            for (int i = instructions.size() - 1; i >= 0; i--) {
                Instruction instruction = instructions.get(i);
                if (instruction instanceof Store store && store.address() instanceof LocalVar local) {
                    if (!live.contains(local)) {
                        removed = true;
                        continue;
                    }
                    addLocalUses(live, store.value());
                    live.remove(local);
                    keptReversed.add(instruction);
                    continue;
                }

                addInstructionUses(live, instruction);
                keptReversed.add(instruction);
            }

            if (removed) {
                Collections.reverse(keptReversed);
                block.replaceInstructions(keptReversed);
            }
        }
        return removed;
    }

    private static Map<BasicBlock, BlockUseDef> computeUseDefs(Function function) {
        Map<BasicBlock, BlockUseDef> useDefs = new IdentityHashMap<>();
        for (BasicBlock block : function.blocks()) {
            Set<LocalVar> uses = identitySet();
            Set<LocalVar> defs = identitySet();
            for (Instruction instruction : block.allInstructions()) {
                if (instruction instanceof Store store && store.address() instanceof LocalVar local) {
                    addLocalUseIfNotDefined(uses, defs, store.value());
                    defs.add(local);
                } else {
                    for (IRValue operand : instruction.operands()) {
                        addLocalUseIfNotDefined(uses, defs, operand);
                    }
                }
            }
            useDefs.put(block, new BlockUseDef(uses, defs));
        }
        return useDefs;
    }

    private static void addInstructionUses(Set<LocalVar> live, Instruction instruction) {
        if (instruction instanceof Store store && store.address() instanceof LocalVar) {
            addLocalUses(live, store.value());
            return;
        }
        for (IRValue operand : instruction.operands()) {
            addLocalUses(live, operand);
        }
    }

    private static void addLocalUseIfNotDefined(Set<LocalVar> uses, Set<LocalVar> defs, IRValue value) {
        if (value instanceof LocalVar local && !defs.contains(local)) {
            uses.add(local);
        }
    }

    private static void addLocalUses(Set<LocalVar> live, IRValue value) {
        if (value instanceof LocalVar local) {
            live.add(local);
        }
    }

    private static Set<LocalVar> identitySet() {
        return Collections.newSetFromMap(new IdentityHashMap<>());
    }

    private static boolean sameSet(Set<LocalVar> left, Set<LocalVar> right) {
        return left.size() == right.size() && left.containsAll(right);
    }

    private record BlockUseDef(Set<LocalVar> uses, Set<LocalVar> defs) {
    }
}
