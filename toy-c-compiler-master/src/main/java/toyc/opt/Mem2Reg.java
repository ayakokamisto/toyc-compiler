package toyc.opt;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Deque;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import toyc.common.Type;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.Alloca;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.Branch;
import toyc.ir.inst.Call;
import toyc.ir.inst.Compare;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Load;
import toyc.ir.inst.LoadImm;
import toyc.ir.inst.Move;
import toyc.ir.inst.Phi;
import toyc.ir.inst.Return;
import toyc.ir.inst.Store;
import toyc.ir.inst.UnaryOp;
import toyc.ir.value.Constant;
import toyc.ir.value.IRValue;
import toyc.ir.value.LocalVar;
import toyc.ir.value.Temp;

final class Mem2Reg {
    private Mem2Reg() {
    }

    static boolean promote(Function function) {
        Set<LocalVar> promotable = findPromotableLocals(function);
        if (promotable.isEmpty()) {
            return false;
        }

        ControlFlowGraph cfg = ControlFlowGraph.build(function);
        DominatorTree domTree = DominatorTree.build(function, cfg);
        Map<BasicBlock, Map<LocalVar, Phi>> phiOf = insertPhis(function, cfg, domTree, promotable);
        RenameState state = new RenameState(nextTempIndex(function));
        for (LocalVar local : promotable) {
            IRValue initial = local.isParameter() ? local : Constant.of(0);
            state.values.put(local, new ArrayDeque<>(List.of(initial)));
        }
        rename(function.entryBlock(), function, cfg, domTree, promotable, phiOf, state);
        return true;
    }

    private static Set<LocalVar> findPromotableLocals(Function function) {
        Set<LocalVar> locals = Collections.newSetFromMap(new IdentityHashMap<>());
        locals.addAll(function.locals().values());
        Set<LocalVar> rejected = Collections.newSetFromMap(new IdentityHashMap<>());
        for (BasicBlock block : function.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                if (instruction instanceof Load load && load.address() instanceof LocalVar) {
                    continue;
                }
                if (instruction instanceof Store store && store.address() instanceof LocalVar) {
                    continue;
                }
                if (instruction instanceof Alloca alloca && alloca.result() instanceof LocalVar) {
                    continue;
                }
                for (IRValue operand : instruction.operands()) {
                    if (operand instanceof LocalVar local) {
                        rejected.add(local);
                    }
                }
            }
        }
        locals.removeAll(rejected);
        return locals;
    }

    private static Map<BasicBlock, Map<LocalVar, Phi>> insertPhis(
            Function function,
            ControlFlowGraph cfg,
            DominatorTree domTree,
            Set<LocalVar> promotable) {
        Map<LocalVar, Set<BasicBlock>> defBlocks = new IdentityHashMap<>();
        for (LocalVar local : promotable) {
            defBlocks.put(local, Collections.newSetFromMap(new IdentityHashMap<>()));
            if (local.isParameter()) {
                defBlocks.get(local).add(function.entryBlock());
            }
        }
        for (BasicBlock block : function.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                if (instruction instanceof Store store && store.address() instanceof LocalVar local && promotable.contains(local)) {
                    defBlocks.get(local).add(block);
                }
            }
        }

        Map<BasicBlock, Map<LocalVar, Phi>> phiOf = new IdentityHashMap<>();
        int nextTemp = nextTempIndex(function);
        for (LocalVar local : promotable) {
            Deque<BasicBlock> worklist = new ArrayDeque<>(defBlocks.get(local));
            Set<BasicBlock> hasPhi = Collections.newSetFromMap(new IdentityHashMap<>());
            while (!worklist.isEmpty()) {
                BasicBlock block = worklist.removeFirst();
                for (BasicBlock frontier : domTree.dominanceFrontier(block, cfg)) {
                    if (hasPhi.add(frontier)) {
                        Phi phi = new Phi(new Temp(nextTemp++, Type.INT));
                        frontier.addPhi(phi);
                        phiOf.computeIfAbsent(frontier, ignored -> new IdentityHashMap<>()).put(local, phi);
                        if (!defBlocks.get(local).contains(frontier)) {
                            worklist.addLast(frontier);
                        }
                    }
                }
            }
        }
        return phiOf;
    }

    private static void rename(
            BasicBlock block,
            Function function,
            ControlFlowGraph cfg,
            DominatorTree domTree,
            Set<LocalVar> promotable,
            Map<BasicBlock, Map<LocalVar, Phi>> phiOf,
            RenameState state) {
        List<LocalVar> pushedLocals = new ArrayList<>();
        Map<IRValue, IRValue> oldReplacements = new IdentityHashMap<>();

        Map<LocalVar, Phi> blockPhis = phiOf.getOrDefault(block, Map.of());
        for (Map.Entry<LocalVar, Phi> entry : blockPhis.entrySet()) {
            pushValue(state, entry.getKey(), entry.getValue().result());
            pushedLocals.add(entry.getKey());
        }

        List<Instruction> rewritten = new ArrayList<>();
        for (Instruction instruction : block.instructions()) {
            if (instruction instanceof Alloca alloca && alloca.result() instanceof LocalVar local && promotable.contains(local)) {
                continue;
            }
            if (instruction instanceof Load load && load.address() instanceof LocalVar local && promotable.contains(local)) {
                rememberReplacement(state, oldReplacements, load.result(), currentValue(state, local));
                continue;
            }
            if (instruction instanceof Store store && store.address() instanceof LocalVar local && promotable.contains(local)) {
                IRValue value = resolve(store.value(), state.replacements);
                pushValue(state, local, value);
                pushedLocals.add(local);
                continue;
            }
            Instruction next = rewriteInstruction(instruction, state.replacements);
            rewritten.add(next);
        }
        block.replaceBody(rewritten);
        if (block.terminator() != null) {
            block.setTerminator(rewriteInstruction(block.terminator(), state.replacements));
        }

        for (BasicBlock successor : cfg.successors(block)) {
            Map<LocalVar, Phi> successorPhis = phiOf.get(successor);
            if (successorPhis == null) {
                continue;
            }
            for (Map.Entry<LocalVar, Phi> entry : successorPhis.entrySet()) {
                entry.getValue().addIncoming(block.label(), currentValue(state, entry.getKey()));
            }
        }

        for (BasicBlock child : domTree.children(block)) {
            rename(child, function, cfg, domTree, promotable, phiOf, state);
        }

        for (int i = pushedLocals.size() - 1; i >= 0; i--) {
            state.values.get(pushedLocals.get(i)).pop();
        }
        for (Map.Entry<IRValue, IRValue> entry : oldReplacements.entrySet()) {
            if (entry.getValue() == null) {
                state.replacements.remove(entry.getKey());
            } else {
                state.replacements.put(entry.getKey(), entry.getValue());
            }
        }
    }

    private static Instruction rewriteInstruction(Instruction instruction, Map<IRValue, IRValue> replacements) {
        if (instruction instanceof BinaryOp binary) {
            return new BinaryOp(binary.result(), binary.op(), resolve(binary.left(), replacements), resolve(binary.right(), replacements));
        }
        if (instruction instanceof Compare compare) {
            return new Compare(compare.result(), compare.predicate(), resolve(compare.left(), replacements), resolve(compare.right(), replacements));
        }
        if (instruction instanceof UnaryOp unary) {
            return new UnaryOp(unary.result(), unary.op(), resolve(unary.value(), replacements));
        }
        if (instruction instanceof Move move) {
            return new Move(move.result(), resolve(move.value(), replacements));
        }
        if (instruction instanceof Load load) {
            return new Load(load.result(), resolve(load.address(), replacements));
        }
        if (instruction instanceof Store store) {
            return new Store(resolve(store.value(), replacements), resolve(store.address(), replacements));
        }
        if (instruction instanceof Call call) {
            List<IRValue> args = new ArrayList<>();
            for (IRValue arg : call.args()) {
                args.add(resolve(arg, replacements));
            }
            return new Call(call.result(), call.functionName(), call.returnType(), args);
        }
        if (instruction instanceof CondBranch branch) {
            return new CondBranch(resolve(branch.condition(), replacements), branch.trueTarget(), branch.falseTarget());
        }
        if (instruction instanceof Return ret && ret.value() != null) {
            return new Return(resolve(ret.value(), replacements));
        }
        if (instruction instanceof GlobalAddr || instruction instanceof LoadImm || instruction instanceof Branch || instruction instanceof Return) {
            return instruction;
        }
        return instruction;
    }

    private static void pushValue(RenameState state, LocalVar local, IRValue value) {
        state.values.computeIfAbsent(local, ignored -> new ArrayDeque<>()).push(value);
    }

    private static IRValue currentValue(RenameState state, LocalVar local) {
        Deque<IRValue> values = state.values.get(local);
        return values == null || values.isEmpty() ? Constant.of(0) : values.peek();
    }

    private static void rememberReplacement(
            RenameState state,
            Map<IRValue, IRValue> oldReplacements,
            IRValue from,
            IRValue to) {
        if (!oldReplacements.containsKey(from)) {
            oldReplacements.put(from, state.replacements.get(from));
        }
        state.replacements.put(from, to);
    }

    private static IRValue resolve(IRValue value, Map<IRValue, IRValue> replacements) {
        IRValue current = value;
        while (true) {
            IRValue next = replacements.get(current);
            if (next == null || next == current) {
                return current;
            }
            current = next;
        }
    }

    private static int nextTempIndex(Function function) {
        int max = -1;
        for (BasicBlock block : function.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                if (instruction.result() instanceof Temp temp) {
                    max = Math.max(max, temp.index());
                }
            }
        }
        return max + 1;
    }

    private static final class RenameState {
        private final Map<LocalVar, Deque<IRValue>> values = new IdentityHashMap<>();
        private final Map<IRValue, IRValue> replacements = new IdentityHashMap<>();

        RenameState(int nextTemp) {
        }
    }
}
