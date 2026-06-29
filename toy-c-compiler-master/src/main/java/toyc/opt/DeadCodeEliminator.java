package toyc.opt;

import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Set;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.Call;
import toyc.ir.inst.Compare;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Load;
import toyc.ir.inst.LoadImm;
import toyc.ir.inst.Move;
import toyc.ir.inst.UnaryOp;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

final class DeadCodeEliminator {
    private DeadCodeEliminator() {
    }

    static boolean eliminate(Function function, FunctionEffects effects) {
        Set<IRValue> used = Collections.newSetFromMap(new IdentityHashMap<>());
        for (BasicBlock block : function.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                used.addAll(instruction.operands());
            }
        }

        boolean changed = false;
        for (BasicBlock block : function.blocks()) {
            List<Instruction> kept = new ArrayList<>();
            for (Instruction instruction : block.instructions()) {
                if (isRemovable(instruction, used, effects)) {
                    changed = true;
                    continue;
                }
                kept.add(instruction);
            }
            if (kept.size() != block.instructions().size()) {
                block.replaceBody(kept);
            }
        }
        return changed;
    }

    private static boolean isRemovable(Instruction instruction, Set<IRValue> used, FunctionEffects effects) {
        IRValue result = instruction.result();
        if (result instanceof Temp && !used.contains(result) && isPureTempDef(instruction)) {
            return true;
        }
        if (instruction instanceof Call call && !effects.mayWriteGlobal(call.functionName())) {
            return call.result() == null || !used.contains(call.result());
        }
        return false;
    }

    private static boolean isPureTempDef(Instruction instruction) {
        return instruction instanceof LoadImm
                || instruction instanceof Move
                || instruction instanceof UnaryOp
                || instruction instanceof BinaryOp
                || instruction instanceof Compare
                || instruction instanceof GlobalAddr
                || instruction instanceof Load;
    }
}
