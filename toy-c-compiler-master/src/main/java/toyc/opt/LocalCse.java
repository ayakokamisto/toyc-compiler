package toyc.opt;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.Compare;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Move;
import toyc.ir.inst.UnaryOp;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

final class LocalCse {
    private LocalCse() {
    }

    static boolean eliminate(Function function) {
        boolean changed = false;
        for (BasicBlock block : function.blocks()) {
            changed |= eliminateInBlock(block);
        }
        return changed;
    }

    private static boolean eliminateInBlock(BasicBlock block) {
        Map<ExpressionKey, IRValue> available = new HashMap<>();
        List<Instruction> rewritten = new ArrayList<>();
        boolean changed = false;

        for (Instruction instruction : block.instructions()) {
            ExpressionKey key = expressionKey(instruction);
            if (key != null) {
                IRValue existing = available.get(key);
                if (existing != null) {
                    rewritten.add(new Move((Temp) instruction.result(), existing));
                    changed = true;
                    continue;
                }
                available.put(key, instruction.result());
            }
            rewritten.add(instruction);
        }

        if (changed) {
            block.replaceBody(rewritten);
        }
        return changed;
    }

    private static ExpressionKey expressionKey(Instruction instruction) {
        if (instruction instanceof UnaryOp unary) {
            return new ExpressionKey(ExpressionKind.UNARY, unary.op(), null, unary.value(), null);
        }
        if (instruction instanceof BinaryOp binary) {
            return new ExpressionKey(ExpressionKind.BINARY, binary.op(), null, binary.left(), binary.right());
        }
        if (instruction instanceof Compare compare) {
            return new ExpressionKey(ExpressionKind.COMPARE, null, compare.predicate(), compare.left(), compare.right());
        }
        if (instruction instanceof GlobalAddr globalAddr) {
            return new ExpressionKey(ExpressionKind.GLOBAL_ADDR, null, null, globalAddr.global(), null);
        }
        return null;
    }

    private enum ExpressionKind {
        UNARY, BINARY, COMPARE, GLOBAL_ADDR
    }

    private record ExpressionKey(ExpressionKind kind, Object op, Object predicate, IRValue left, IRValue right) {
    }
}
