package toyc.opt;

import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.LoadImm;
import toyc.ir.inst.Move;
import toyc.ir.value.Constant;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

final class LinearExpressionOptimizer {
    private LinearExpressionOptimizer() {
    }

    static boolean optimize(Function function) {
        boolean changed = false;
        for (BasicBlock block : function.blocks()) {
            changed |= optimizeBlock(block);
        }
        return changed;
    }

    private static boolean optimizeBlock(BasicBlock block) {
        Map<IRValue, LinearValue> linearValues = new IdentityHashMap<>();
        List<Instruction> rewritten = new ArrayList<>();
        boolean changed = false;

        for (Instruction instruction : block.instructions()) {
            if (instruction.result() != null) {
                linearValues.remove(instruction.result());
            }
            Instruction next = rewrite(instruction, linearValues);
            changed |= next != instruction;
            record(next, linearValues);
            rewritten.add(next);
        }

        if (changed) {
            block.replaceBody(rewritten);
        }
        return changed;
    }

    private static Instruction rewrite(Instruction instruction, Map<IRValue, LinearValue> linearValues) {
        if (!(instruction instanceof BinaryOp binary)) {
            return instruction;
        }
        if (binary.op() != BinaryOp.Op.ADD && binary.op() != BinaryOp.Op.SUB) {
            return instruction;
        }

        LinearValue left = linearValue(binary.left(), linearValues);
        LinearValue right = linearValue(binary.right(), linearValues);
        LinearValue folded = binary.op() == BinaryOp.Op.ADD ? add(left, right) : subtract(left, right);
        if (folded == null) {
            return instruction;
        }
        return materialize(binary.result(), folded, instruction);
    }

    private static void record(Instruction instruction, Map<IRValue, LinearValue> linearValues) {
        if (instruction instanceof LoadImm loadImm) {
            linearValues.put(loadImm.result(), new LinearValue(null, loadImm.constant().value()));
            return;
        }
        if (instruction instanceof Move move) {
            linearValues.put(move.result(), linearValue(move.value(), linearValues));
            return;
        }
        if (instruction instanceof BinaryOp binary && (binary.op() == BinaryOp.Op.ADD || binary.op() == BinaryOp.Op.SUB)) {
            LinearValue left = linearValue(binary.left(), linearValues);
            LinearValue right = linearValue(binary.right(), linearValues);
            LinearValue folded = binary.op() == BinaryOp.Op.ADD ? add(left, right) : subtract(left, right);
            if (folded != null) {
                linearValues.put(binary.result(), folded);
            }
        }
    }

    private static LinearValue linearValue(IRValue value, Map<IRValue, LinearValue> linearValues) {
        if (value instanceof Constant constant) {
            return new LinearValue(null, constant.value());
        }
        LinearValue known = linearValues.get(value);
        return known == null ? new LinearValue(value, 0) : known;
    }

    private static LinearValue add(LinearValue left, LinearValue right) {
        if (left.base() == null) {
            return new LinearValue(right.base(), left.constant() + right.constant());
        }
        if (right.base() == null) {
            return new LinearValue(left.base(), left.constant() + right.constant());
        }
        return null;
    }

    private static LinearValue subtract(LinearValue left, LinearValue right) {
        if (right.base() == null) {
            return new LinearValue(left.base(), left.constant() - right.constant());
        }
        if (left.base() == right.base()) {
            return new LinearValue(null, left.constant() - right.constant());
        }
        return null;
    }

    private static Instruction materialize(Temp result, LinearValue value, Instruction original) {
        if (value.base() == null) {
            return new LoadImm(result, value.constant());
        }
        if (value.constant() == 0) {
            return new Move(result, value.base());
        }
        if (original instanceof BinaryOp binary && binary.left() == value.base()
                && isConstantValue(binary.right(), value.constant())
                && binary.op() == BinaryOp.Op.ADD) {
            return original;
        }
        return new BinaryOp(result, BinaryOp.Op.ADD, value.base(), Constant.of(value.constant()));
    }

    private static boolean isConstantValue(IRValue value, int expected) {
        return value instanceof Constant constant && constant.value() == expected;
    }

    private record LinearValue(IRValue base, int constant) {
    }
}
