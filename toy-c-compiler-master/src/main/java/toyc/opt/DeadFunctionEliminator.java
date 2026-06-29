package toyc.opt;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import toyc.ir.block.Function;
import toyc.ir.block.Module;
import toyc.ir.inst.Call;
import toyc.ir.inst.Instruction;

final class DeadFunctionEliminator {
    private DeadFunctionEliminator() {
    }

    static boolean eliminate(Module module) {
        Function main = module.mainFunction();
        if (main == null) {
            return false;
        }

        Map<String, Function> functions = new HashMap<>();
        for (Function function : module.functions()) {
            functions.put(function.name(), function);
        }

        Set<Function> reachable = new HashSet<>();
        ArrayDeque<Function> worklist = new ArrayDeque<>();
        worklist.add(main);
        while (!worklist.isEmpty()) {
            Function function = worklist.removeFirst();
            if (!reachable.add(function)) {
                continue;
            }
            for (var block : function.blocks()) {
                for (Instruction instruction : block.allInstructions()) {
                    if (instruction instanceof Call call) {
                        Function callee = functions.get(call.functionName());
                        if (callee != null) {
                            worklist.addLast(callee);
                        }
                    }
                }
            }
        }

        if (reachable.size() == module.functions().size()) {
            return false;
        }
        List<Function> kept = new ArrayList<>();
        for (Function function : module.functions()) {
            if (reachable.contains(function)) {
                kept.add(function);
            }
        }
        module.replaceFunctions(kept);
        return true;
    }
}
