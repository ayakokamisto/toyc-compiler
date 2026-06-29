package toyc.opt;

import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import toyc.common.Type;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.Call;
import toyc.ir.inst.Compare;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Load;
import toyc.ir.inst.Move;
import toyc.ir.inst.Return;
import toyc.ir.inst.Store;
import toyc.ir.inst.UnaryOp;
import toyc.ir.value.GlobalVar;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

final class GlobalScalarReplacement {
    private GlobalScalarReplacement() {
    }

    static boolean replace(Function function, FunctionEffects effects) {
        RewriteState state = new RewriteState(nextTempIndex(function));
        if (!collectGlobals(function, state, effects)) {
            return false;
        }
        if (state.globalTemps.isEmpty() || state.dirtyGlobals.isEmpty()) {
            return false;
        }

        boolean changed = false;
        for (BasicBlock block : function.blocks()) {
            List<Instruction> rewritten = new ArrayList<>();
            for (Instruction instruction : block.instructions()) {
                Instruction next = rewriteInstruction(instruction, state);
                if (next != null) {
                    rewritten.add(next);
                }
                changed |= next != instruction;
            }
            if (block == function.entryBlock()) {
                List<Instruction> withLoads = new ArrayList<>();
                for (Map.Entry<GlobalVar, Temp> entry : state.globalTemps.entrySet()) {
                    withLoads.add(new Load(entry.getValue(), entry.getKey()));
                }
                withLoads.addAll(rewritten);
                rewritten = withLoads;
                changed = true;
            }
            block.replaceBody(rewritten);

            if (block.terminator() instanceof Return) {
                List<Instruction> withStores = new ArrayList<>(block.instructions());
                for (GlobalVar global : state.dirtyGlobals.keySet()) {
                    withStores.add(new Store(state.globalTemps.get(global), global));
                }
                if (!withStores.equals(block.instructions())) {
                    block.replaceBody(withStores);
                    changed = true;
                }
            } else {
                Instruction terminator = rewriteTerminator(block.terminator(), state);
                if (terminator != block.terminator()) {
                    block.setTerminator(terminator);
                    changed = true;
                }
            }
        }
        return changed;
    }

    private static boolean collectGlobals(Function function, RewriteState state, FunctionEffects effects) {
        for (BasicBlock block : function.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                if (instruction instanceof GlobalAddr globalAddr) {
                    state.addressGlobals.put(globalAddr.result(), globalAddr.global());
                    state.globalTemps.computeIfAbsent(globalAddr.global(), ignored -> state.newTemp());
                    continue;
                }
                if (instruction instanceof Move move && state.addressGlobals.containsKey(move.value())) {
                    state.addressGlobals.put(move.result(), state.addressGlobals.get(move.value()));
                    continue;
                }
                if (instruction instanceof Load load && globalForAddress(load.address(), state) != null) {
                    continue;
                }
                if (instruction instanceof Store store) {
                    GlobalVar global = globalForAddress(store.address(), state);
                    if (global != null) {
                        state.dirtyGlobals.put(global, Boolean.TRUE);
                        continue;
                    }
                }
                if (instruction instanceof Call call
                        && (effects.mayReadGlobal(call.functionName()) || effects.mayWriteGlobal(call.functionName()))) {
                    return false;
                }
                for (IRValue operand : instruction.operands()) {
                    if (state.addressGlobals.containsKey(operand)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    private static Instruction rewriteInstruction(Instruction instruction, RewriteState state) {
        if (instruction instanceof GlobalAddr) {
            return null;
        }
        if (instruction instanceof Move move && state.addressGlobals.containsKey(move.value())) {
            state.addressGlobals.put(move.result(), state.addressGlobals.get(move.value()));
            return null;
        }
        if (instruction instanceof Load load) {
            GlobalVar global = globalForAddress(load.address(), state);
            if (global != null) {
                return new Move(load.result(), state.globalTemps.get(global));
            }
        }
        if (instruction instanceof Store store) {
            GlobalVar global = globalForAddress(store.address(), state);
            if (global != null) {
                state.dirtyGlobals.put(global, Boolean.TRUE);
                return new Move(state.globalTemps.get(global), store.value());
            }
        }
        return rewriteOperands(instruction, state);
    }

    private static Instruction rewriteTerminator(Instruction instruction, RewriteState state) {
        if (instruction == null) {
            return null;
        }
        return rewriteOperands(instruction, state);
    }

    private static Instruction rewriteOperands(Instruction instruction, RewriteState state) {
        if (instruction instanceof BinaryOp binary) {
            return new BinaryOp(binary.result(), binary.op(), resolve(binary.left(), state), resolve(binary.right(), state));
        }
        if (instruction instanceof Compare compare) {
            return new Compare(compare.result(), compare.predicate(), resolve(compare.left(), state), resolve(compare.right(), state));
        }
        if (instruction instanceof UnaryOp unary) {
            return new UnaryOp(unary.result(), unary.op(), resolve(unary.value(), state));
        }
        if (instruction instanceof Move move) {
            return new Move(move.result(), resolve(move.value(), state));
        }
        if (instruction instanceof CondBranch branch) {
            return new CondBranch(resolve(branch.condition(), state), branch.trueTarget(), branch.falseTarget());
        }
        if (instruction instanceof Return ret && ret.value() != null) {
            return new Return(resolve(ret.value(), state));
        }
        return instruction;
    }

    private static IRValue resolve(IRValue value, RewriteState state) {
        GlobalVar global = globalForAddress(value, state);
        return global == null ? value : state.globalTemps.get(global);
    }

    private static GlobalVar globalForAddress(IRValue value, RewriteState state) {
        if (value instanceof GlobalVar global) {
            return global;
        }
        return state.addressGlobals.get(value);
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

    private static final class RewriteState {
        private int nextTemp;
        private final Map<IRValue, GlobalVar> addressGlobals = new IdentityHashMap<>();
        private final Map<GlobalVar, Temp> globalTemps = new LinkedHashMap<>();
        private final Map<GlobalVar, Boolean> dirtyGlobals = new LinkedHashMap<>();

        RewriteState(int nextTemp) {
            this.nextTemp = nextTemp;
        }

        Temp newTemp() {
            return new Temp(nextTemp++, Type.INT);
        }
    }
}
