package toyc.opt;

import java.util.ArrayList;
import java.util.List;
import toyc.common.Type;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.Branch;
import toyc.ir.inst.Call;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Move;
import toyc.ir.inst.Return;
import toyc.ir.inst.Store;
import toyc.ir.value.Temp;

final class TailRecursionEliminator {
    private TailRecursionEliminator() {
    }

    static boolean eliminate(Function function) {
        boolean changed = false;
        int nextTemp = nextTempIndex(function);
        for (BasicBlock block : function.blocks()) {
            if (!(block.terminator() instanceof Return ret) || block.instructions().isEmpty()) {
                continue;
            }
            Instruction last = block.instructions().get(block.instructions().size() - 1);
            if (!(last instanceof Call call) || !function.name().equals(call.functionName())) {
                continue;
            }
            if (call.result() == null || ret.value() != call.result()
                    || call.args().size() != function.parameters().size()) {
                continue;
            }

            List<Instruction> rewritten = new ArrayList<>(block.instructions().subList(0, block.instructions().size() - 1));
            List<Temp> argumentCopies = new ArrayList<>(call.args().size());
            for (var arg : call.args()) {
                Temp copy = new Temp(nextTemp++, Type.INT);
                argumentCopies.add(copy);
                rewritten.add(new Move(copy, arg));
            }
            for (int i = 0; i < argumentCopies.size(); i++) {
                rewritten.add(new Store(argumentCopies.get(i), function.parameters().get(i)));
            }
            block.replaceBody(rewritten);
            block.setTerminator(new Branch(function.entryBlock().label()));
            changed = true;
        }
        return changed;
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
}
