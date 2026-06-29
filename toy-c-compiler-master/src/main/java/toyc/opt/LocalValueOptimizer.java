package toyc.opt;

import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.Call;
import toyc.ir.inst.Compare;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Load;
import toyc.ir.inst.LoadImm;
import toyc.ir.inst.Move;
import toyc.ir.inst.Return;
import toyc.ir.inst.Store;
import toyc.ir.inst.UnaryOp;
import toyc.ir.value.Constant;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

final class LocalValueOptimizer {
    private LocalValueOptimizer() {
    }

    static boolean optimize(Function function) {
        boolean changed = false;
        for (BasicBlock block : function.blocks()) {
            changed |= rewriteBlock(block);
        }
        changed |= eliminateDeadTemps(function);
        return changed;
    }

    private static boolean rewriteBlock(BasicBlock block) {
        Map<IRValue, IRValue> replacements = new IdentityHashMap<>();
        List<Instruction> rewritten = new ArrayList<>();
        boolean changed = false;

        for (Instruction instruction : block.allInstructions()) {
            Instruction next = rewriteInstruction(instruction, replacements);
            changed |= next != instruction;
            IRValue result = next.result();
            if (result != null) {
                replacements.remove(result);
            }
            recordReplacement(next, replacements);
            rewritten.add(next);
        }

        if (changed) {
            block.replaceInstructions(rewritten);
        }
        return changed;
    }

    private static Instruction rewriteInstruction(Instruction instruction, Map<IRValue, IRValue> replacements) {
        if (instruction instanceof LoadImm loadImm) {
            return loadImm;
        }
        if (instruction instanceof Move move) {
            IRValue value = resolve(move.value(), replacements);
            Constant constant = asConstant(value);
            if (constant != null) {
                return new LoadImm(move.result(), constant);
            }
            return value == move.value() ? instruction : new Move(move.result(), value);
        }
        if (instruction instanceof UnaryOp unary) {
            IRValue value = resolve(unary.value(), replacements);
            Constant constant = asConstant(value);
            if (constant != null) {
                return new LoadImm(unary.result(), evalUnary(unary.op(), constant.value()));
            }
            return value == unary.value() ? instruction : new UnaryOp(unary.result(), unary.op(), value);
        }
        if (instruction instanceof BinaryOp binary) {
            IRValue left = resolve(binary.left(), replacements);
            IRValue right = resolve(binary.right(), replacements);
            Constant leftConstant = asConstant(left);
            Constant rightConstant = asConstant(right);
            if (leftConstant != null && rightConstant != null) {
                Integer value = evalBinary(binary.op(), leftConstant.value(), rightConstant.value());
                if (value != null) {
                    return new LoadImm(binary.result(), value);
                }
            }
            IRValue simplified = simplifyBinary(binary.op(), left, right);
            if (simplified != null) {
                return copyValue(binary.result(), simplified);
            }
            return left == binary.left() && right == binary.right()
                    ? instruction
                    : new BinaryOp(binary.result(), binary.op(), left, right);
        }
        if (instruction instanceof Compare compare) {
            IRValue left = resolve(compare.left(), replacements);
            IRValue right = resolve(compare.right(), replacements);
            Constant leftConstant = asConstant(left);
            Constant rightConstant = asConstant(right);
            if (leftConstant != null && rightConstant != null) {
                return new LoadImm(compare.result(), evalCompare(compare.predicate(), leftConstant.value(), rightConstant.value()));
            }
            return left == compare.left() && right == compare.right()
                    ? instruction
                    : new Compare(compare.result(), compare.predicate(), left, right);
        }
        if (instruction instanceof Load load) {
            IRValue address = resolve(load.address(), replacements);
            return address == load.address() ? instruction : new Load(load.result(), address);
        }
        if (instruction instanceof Store store) {
            IRValue value = resolve(store.value(), replacements);
            IRValue address = resolve(store.address(), replacements);
            return value == store.value() && address == store.address() ? instruction : new Store(value, address);
        }
        if (instruction instanceof Call call) {
            List<IRValue> args = resolveAll(call.args(), replacements);
            return sameValues(args, call.args()) ? instruction : new Call(call.result(), call.functionName(), call.returnType(), args);
        }
        if (instruction instanceof CondBranch branch) {
            IRValue condition = resolve(branch.condition(), replacements);
            return condition == branch.condition()
                    ? instruction
                    : new CondBranch(condition, branch.trueTarget(), branch.falseTarget());
        }
        if (instruction instanceof Return ret) {
            if (ret.value() == null) {
                return instruction;
            }
            IRValue value = resolve(ret.value(), replacements);
            return value == ret.value() ? instruction : new Return(value);
        }
        return instruction;
    }

    private static void recordReplacement(Instruction instruction, Map<IRValue, IRValue> replacements) {
        if (instruction instanceof LoadImm loadImm) {
            replacements.put(loadImm.result(), loadImm.constant());
            return;
        }
        if (instruction instanceof Move move) {
            replacements.put(move.result(), move.value());
            return;
        }
        if (instruction instanceof UnaryOp unary && unary.op() == UnaryOp.Op.NEG && unary.value() instanceof Constant constant) {
            replacements.put(unary.result(), Constant.of(-constant.value()));
        }
    }

    private static boolean eliminateDeadTemps(Function function) {
        Set<IRValue> used = Collections.newSetFromMap(new IdentityHashMap<>());
        for (BasicBlock block : function.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                used.addAll(instruction.operands());
            }
        }

        boolean changed = false;
        for (BasicBlock block : function.blocks()) {
            List<Instruction> kept = new ArrayList<>();
            for (Instruction instruction : block.allInstructions()) {
                if (isPureTempDef(instruction) && !used.contains(instruction.result())) {
                    changed = true;
                    continue;
                }
                kept.add(instruction);
            }
            if (kept.size() != block.allInstructions().size()) {
                block.replaceInstructions(kept);
            }
        }
        return changed;
    }

    private static boolean isPureTempDef(Instruction instruction) {
        if (!(instruction.result() instanceof Temp)) {
            return false;
        }
        return instruction instanceof LoadImm
                || instruction instanceof Move
                || instruction instanceof UnaryOp
                || instruction instanceof BinaryOp
                || instruction instanceof Compare
                || instruction instanceof GlobalAddr
                || instruction instanceof Load;
    }

    private static Instruction copyValue(Temp result, IRValue value) {
        if (value instanceof Constant constant) {
            return new LoadImm(result, constant);
        }
        return new Move(result, value);
    }

    private static IRValue simplifyBinary(BinaryOp.Op op, IRValue left, IRValue right) {
        Constant leftConstant = asConstant(left);
        Constant rightConstant = asConstant(right);
        return switch (op) {
            case ADD -> {
                if (isConst(rightConstant, 0)) {
                    yield left;
                }
                if (isConst(leftConstant, 0)) {
                    yield right;
                }
                yield null;
            }
            case SUB -> isConst(rightConstant, 0) ? left : null;
            case MUL -> {
                if (isConst(rightConstant, 0) || isConst(leftConstant, 0)) {
                    yield Constant.of(0);
                }
                if (isConst(rightConstant, 1)) {
                    yield left;
                }
                if (isConst(leftConstant, 1)) {
                    yield right;
                }
                yield null;
            }
            case DIV -> isConst(rightConstant, 1) ? left : null;
            case MOD -> isConst(rightConstant, 1) ? Constant.of(0) : null;
        };
    }

    private static Integer evalBinary(BinaryOp.Op op, int left, int right) {
        return switch (op) {
            case ADD -> left + right;
            case SUB -> left - right;
            case MUL -> left * right;
            case DIV -> right == 0 ? null : left / right;
            case MOD -> right == 0 ? null : left % right;
        };
    }

    private static int evalUnary(UnaryOp.Op op, int value) {
        return switch (op) {
            case NEG -> -value;
            case NOT -> value == 0 ? 1 : 0;
        };
    }

    private static int evalCompare(Compare.Predicate predicate, int left, int right) {
        boolean result = switch (predicate) {
            case LT -> left < right;
            case GT -> left > right;
            case LE -> left <= right;
            case GE -> left >= right;
            case EQ -> left == right;
            case NE -> left != right;
        };
        return result ? 1 : 0;
    }

    private static IRValue resolve(IRValue value, Map<IRValue, IRValue> replacements) {
        IRValue current = value;
        while (true) {
            IRValue replacement = replacements.get(current);
            if (replacement == null || replacement == current) {
                return current;
            }
            current = replacement;
        }
    }

    private static List<IRValue> resolveAll(List<IRValue> values, Map<IRValue, IRValue> replacements) {
        List<IRValue> resolved = new ArrayList<>(values.size());
        for (IRValue value : values) {
            resolved.add(resolve(value, replacements));
        }
        return resolved;
    }

    private static boolean sameValues(List<IRValue> left, List<IRValue> right) {
        if (left.size() != right.size()) {
            return false;
        }
        for (int i = 0; i < left.size(); i++) {
            if (left.get(i) != right.get(i)) {
                return false;
            }
        }
        return true;
    }

    private static Constant asConstant(IRValue value) {
        return value instanceof Constant constant ? constant : null;
    }

    private static boolean isConst(Constant constant, int value) {
        return constant != null && constant.value() == value;
    }
}
