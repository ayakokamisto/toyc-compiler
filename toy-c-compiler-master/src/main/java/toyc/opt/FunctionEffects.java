package toyc.opt;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.Call;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Load;
import toyc.ir.inst.Move;
import toyc.ir.inst.Store;
import toyc.ir.value.GlobalVar;
import toyc.ir.value.IRValue;

final class FunctionEffects {
    private final Map<String, Effect> effectsByFunction;

    private FunctionEffects(Map<String, Effect> effectsByFunction) {
        this.effectsByFunction = effectsByFunction;
    }

    static FunctionEffects analyze(toyc.ir.block.Module module) {
        Map<String, Effect> effects = new HashMap<>();
        Map<String, List<String>> calls = new HashMap<>();

        for (Function function : module.functions()) {
            Effect effect = new Effect(false, false);
            List<String> callees = new ArrayList<>();
            Map<IRValue, GlobalVar> globalAddresses = new IdentityHashMap<>();
            for (BasicBlock block : function.blocks()) {
                for (Instruction instruction : block.allInstructions()) {
                    if (instruction instanceof GlobalAddr globalAddr) {
                        globalAddresses.put(globalAddr.result(), globalAddr.global());
                        continue;
                    }
                    if (instruction instanceof Move move) {
                        GlobalVar global = globalForAddress(move.value(), globalAddresses);
                        if (global != null) {
                            globalAddresses.put(move.result(), global);
                        }
                        continue;
                    }
                    if (instruction instanceof Load load && globalForAddress(load.address(), globalAddresses) != null) {
                        effect = effect.withRead();
                        continue;
                    }
                    if (instruction instanceof Store store && globalForAddress(store.address(), globalAddresses) != null) {
                        effect = effect.withWrite();
                        continue;
                    }
                    if (instruction instanceof Call call) {
                        callees.add(call.functionName());
                    }
                }
            }
            effects.put(function.name(), effect);
            calls.put(function.name(), callees);
        }

        boolean changed;
        do {
            changed = false;
            for (Map.Entry<String, List<String>> entry : calls.entrySet()) {
                Effect current = effects.get(entry.getKey());
                Effect next = current;
                for (String callee : entry.getValue()) {
                    Effect calleeEffect = effects.get(callee);
                    if (calleeEffect == null) {
                        next = new Effect(true, true);
                    } else {
                        next = next.merge(calleeEffect);
                    }
                }
                if (!next.equals(current)) {
                    effects.put(entry.getKey(), next);
                    changed = true;
                }
            }
        } while (changed);

        return new FunctionEffects(effects);
    }

    boolean mayReadGlobal(String functionName) {
        return effect(functionName).mayReadGlobal();
    }

    boolean mayWriteGlobal(String functionName) {
        return effect(functionName).mayWriteGlobal();
    }

    private Effect effect(String functionName) {
        return effectsByFunction.getOrDefault(functionName, new Effect(true, true));
    }

    private static GlobalVar globalForAddress(IRValue value, Map<IRValue, GlobalVar> globalAddresses) {
        if (value instanceof GlobalVar global) {
            return global;
        }
        return globalAddresses.get(value);
    }

    private record Effect(boolean mayReadGlobal, boolean mayWriteGlobal) {
        Effect withRead() {
            return new Effect(true, mayWriteGlobal);
        }

        Effect withWrite() {
            return new Effect(mayReadGlobal, true);
        }

        Effect merge(Effect other) {
            return new Effect(mayReadGlobal || other.mayReadGlobal, mayWriteGlobal || other.mayWriteGlobal);
        }
    }
}
