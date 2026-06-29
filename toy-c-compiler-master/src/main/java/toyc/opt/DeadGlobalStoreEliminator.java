package toyc.opt;

import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Load;
import toyc.ir.inst.Move;
import toyc.ir.inst.Store;
import toyc.ir.value.GlobalVar;
import toyc.ir.value.IRValue;

final class DeadGlobalStoreEliminator {
    private DeadGlobalStoreEliminator() {
    }

    static boolean eliminate(toyc.ir.block.Module module) {
        Set<GlobalVar> readGlobals = Collections.newSetFromMap(new IdentityHashMap<>());
        List<FunctionState> states = new ArrayList<>();
        for (Function function : module.functions()) {
            FunctionState state = collectFunctionState(function);
            states.add(state);
            readGlobals.addAll(state.readGlobals());
        }

        boolean changed = false;
        for (FunctionState state : states) {
            for (BasicBlock block : state.function().blocks()) {
                List<Instruction> kept = new ArrayList<>();
                boolean blockChanged = false;
                for (Instruction instruction : block.instructions()) {
                    if (instruction instanceof Store store) {
                        GlobalVar global = globalForAddress(store.address(), state.addressGlobals());
                        if (global != null && !readGlobals.contains(global)) {
                            blockChanged = true;
                            changed = true;
                            continue;
                        }
                    }
                    kept.add(instruction);
                }
                if (blockChanged) {
                    block.replaceBody(kept);
                }
            }
        }
        return changed;
    }

    private static FunctionState collectFunctionState(Function function) {
        Map<IRValue, GlobalVar> addressGlobals = new IdentityHashMap<>();
        Set<GlobalVar> readGlobals = Collections.newSetFromMap(new IdentityHashMap<>());
        for (BasicBlock block : function.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                if (instruction instanceof GlobalAddr globalAddr) {
                    addressGlobals.put(globalAddr.result(), globalAddr.global());
                    continue;
                }
                if (instruction instanceof Move move) {
                    GlobalVar global = globalForAddress(move.value(), addressGlobals);
                    if (global != null) {
                        addressGlobals.put(move.result(), global);
                    }
                    continue;
                }
                if (instruction instanceof Load load) {
                    GlobalVar global = globalForAddress(load.address(), addressGlobals);
                    if (global != null) {
                        readGlobals.add(global);
                    }
                }
            }
        }
        return new FunctionState(function, addressGlobals, readGlobals);
    }

    private static GlobalVar globalForAddress(IRValue value, Map<IRValue, GlobalVar> addressGlobals) {
        if (value instanceof GlobalVar global) {
            return global;
        }
        return addressGlobals.get(value);
    }

    private record FunctionState(
            Function function,
            Map<IRValue, GlobalVar> addressGlobals,
            Set<GlobalVar> readGlobals) {
    }
}
