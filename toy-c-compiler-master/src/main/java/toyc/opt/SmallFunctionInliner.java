package toyc.opt;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import toyc.common.Type;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.block.Module;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.Call;
import toyc.ir.inst.Compare;
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
import toyc.ir.value.LocalVar;
import toyc.ir.value.Temp;

final class SmallFunctionInliner {
    private static final int MAX_BODY_INSTRUCTIONS = 24;

    private SmallFunctionInliner() {
    }

    static boolean inline(Module module) {
        Map<String, Function> functions = new HashMap<>();
        for (Function function : module.functions()) {
            functions.put(function.name(), function);
        }

        boolean changed = false;
        for (Function caller : module.functions()) {
            changed |= inlineIntoFunction(caller, functions);
        }
        return changed;
    }

    private static boolean inlineIntoFunction(Function caller, Map<String, Function> functions) {
        boolean changed = false;
        InlineContext context = new InlineContext(nextTempIndex(caller));
        for (BasicBlock block : caller.blocks()) {
            List<Instruction> rewritten = new ArrayList<>();
            boolean blockChanged = false;
            for (Instruction instruction : block.instructions()) {
                if (instruction instanceof Call call) {
                    Function callee = functions.get(call.functionName());
                    InlineResult inline = expandIfInlineable(callee, caller, call, context);
                    if (inline != null) {
                        rewritten.addAll(inline.instructions());
                        if (call.result() != null && inline.value() != null) {
                            rewritten.add(new Move(call.result(), inline.value()));
                        }
                        blockChanged = true;
                        continue;
                    }
                }
                rewritten.add(instruction);
            }
            if (blockChanged) {
                block.replaceBody(rewritten);
                changed = true;
            }
        }
        return changed;
    }

    private static boolean canInline(Function callee, Function caller, Call call) {
        if (callee == null || callee == caller) {
            return false;
        }
        if (call.args().size() != callee.parameters().size()) {
            return false;
        }
        if (callee.blocks().size() != 1) {
            return false;
        }
        BasicBlock body = callee.entryBlock();
        if (!body.phis().isEmpty()) {
            return false;
        }
        if (!(body.terminator() instanceof Return ret)) {
            return false;
        }
        if (body.instructions().size() > MAX_BODY_INSTRUCTIONS) {
            return false;
        }
        if (ret.value() != null && !isPureExpressionReturn(ret.value())) {
            return false;
        }
        if (containsRecursiveCall(callee)) {
            return false;
        }
        for (Instruction instruction : body.instructions()) {
            if (!isInlineableInstruction(instruction)) {
                return false;
            }
        }
        return true;
    }

    private static InlineResult expandIfInlineable(
            Function callee,
            Function caller,
            Call call,
            InlineContext context) {
        if (canInline(callee, caller, call)) {
            return expandInline(callee, call, context);
        }
        if (canInlineConditionalReturn(callee, caller, call)) {
            return expandConditionalReturnInline(callee, call, context);
        }
        return null;
    }

    private static boolean canInlineConditionalReturn(Function callee, Function caller, Call call) {
        if (callee == null || callee == caller || call.result() == null || call.args().size() != callee.parameters().size()) {
            return false;
        }
        if (callee.blocks().size() != 3 || containsRecursiveCall(callee)) {
            return false;
        }
        BasicBlock entry = callee.entryBlock();
        if (!entry.phis().isEmpty() || !(entry.terminator() instanceof CondBranch branch)) {
            return false;
        }
        if (entry.instructions().size() > MAX_BODY_INSTRUCTIONS) {
            return false;
        }
        for (Instruction instruction : entry.instructions()) {
            if (!isPureInlineableInstruction(instruction)) {
                return false;
            }
        }
        BasicBlock trueBlock = blockWithLabel(callee, branch.trueTarget());
        BasicBlock falseBlock = blockWithLabel(callee, branch.falseTarget());
        return isReturnOnlyBlock(trueBlock) && isReturnOnlyBlock(falseBlock);
    }

    private static boolean isPureInlineableInstruction(Instruction instruction) {
        return instruction instanceof LoadImm
                || instruction instanceof Move
                || instruction instanceof UnaryOp
                || instruction instanceof BinaryOp
                || instruction instanceof Compare
                || instruction instanceof GlobalAddr
                || instruction instanceof Load;
    }

    private static boolean isReturnOnlyBlock(BasicBlock block) {
        return block != null
                && block.phis().isEmpty()
                && block.instructions().isEmpty()
                && block.terminator() instanceof Return ret
                && ret.value() != null
                && isPureExpressionReturn(ret.value());
    }

    private static BasicBlock blockWithLabel(Function function, toyc.ir.value.Label label) {
        for (BasicBlock block : function.blocks()) {
            if (block.label() == label) {
                return block;
            }
        }
        return null;
    }

    private static boolean containsRecursiveCall(Function callee) {
        for (BasicBlock block : callee.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                if (instruction instanceof Call call && callee.name().equals(call.functionName())) {
                    return true;
                }
            }
        }
        return false;
    }

    private static boolean isPureExpressionReturn(IRValue value) {
        return value instanceof Temp || value instanceof LocalVar || value instanceof Constant;
    }

    private static boolean isInlineableInstruction(Instruction instruction) {
        return instruction instanceof LoadImm
                || instruction instanceof Move
                || instruction instanceof UnaryOp
                || instruction instanceof BinaryOp
                || instruction instanceof Compare
                || instruction instanceof GlobalAddr
                || instruction instanceof Load
                || instruction instanceof Store;
    }

    private static InlineResult expandInline(Function callee, Call call, InlineContext context) {
        BasicBlock body = callee.entryBlock();
        Map<IRValue, IRValue> replacements = new IdentityHashMap<>();
        for (int i = 0; i < callee.parameters().size(); i++) {
            replacements.put(callee.parameters().get(i), call.args().get(i));
        }

        List<Instruction> cloned = new ArrayList<>();
        for (Instruction instruction : body.instructions()) {
            IRValue result = instruction.result();
            if (result instanceof Temp temp && !replacements.containsKey(temp)) {
                replacements.put(temp, context.newTemp(temp.type()));
            }
            cloned.add(cloneInstruction(instruction, replacements));
        }
        IRValue result = resolve(((Return) body.terminator()).value(), replacements);
        return new InlineResult(cloned, result);
    }

    private static InlineResult expandConditionalReturnInline(Function callee, Call call, InlineContext context) {
        BasicBlock entry = callee.entryBlock();
        CondBranch branch = (CondBranch) entry.terminator();
        BasicBlock trueBlock = blockWithLabel(callee, branch.trueTarget());
        BasicBlock falseBlock = blockWithLabel(callee, branch.falseTarget());

        Map<IRValue, IRValue> replacements = new IdentityHashMap<>();
        for (int i = 0; i < callee.parameters().size(); i++) {
            replacements.put(callee.parameters().get(i), call.args().get(i));
        }

        List<Instruction> cloned = new ArrayList<>();
        for (Instruction instruction : entry.instructions()) {
            IRValue result = instruction.result();
            if (result instanceof Temp temp && !replacements.containsKey(temp)) {
                replacements.put(temp, context.newTemp(temp.type()));
            }
            cloned.add(cloneInstruction(instruction, replacements));
        }

        IRValue condition = resolve(branch.condition(), replacements);
        IRValue trueValue = resolve(((Return) trueBlock.terminator()).value(), replacements);
        IRValue falseValue = resolve(((Return) falseBlock.terminator()).value(), replacements);
        Temp diff = context.newTemp(Type.INT);
        Temp scaled = context.newTemp(Type.INT);
        Temp selected = context.newTemp(Type.INT);
        cloned.add(new BinaryOp(diff, BinaryOp.Op.SUB, trueValue, falseValue));
        cloned.add(new BinaryOp(scaled, BinaryOp.Op.MUL, condition, diff));
        cloned.add(new BinaryOp(selected, BinaryOp.Op.ADD, falseValue, scaled));
        return new InlineResult(cloned, selected);
    }

    private static Instruction cloneInstruction(Instruction instruction, Map<IRValue, IRValue> replacements) {
        if (instruction instanceof LoadImm loadImm) {
            return new LoadImm(mappedTemp(loadImm.result(), replacements), loadImm.constant());
        }
        if (instruction instanceof Move move) {
            return new Move(mappedTemp(move.result(), replacements), resolve(move.value(), replacements));
        }
        if (instruction instanceof UnaryOp unary) {
            return new UnaryOp(mappedTemp(unary.result(), replacements), unary.op(), resolve(unary.value(), replacements));
        }
        if (instruction instanceof BinaryOp binary) {
            return new BinaryOp(mappedTemp(binary.result(), replacements), binary.op(), resolve(binary.left(), replacements), resolve(binary.right(), replacements));
        }
        if (instruction instanceof Compare compare) {
            return new Compare(mappedTemp(compare.result(), replacements), compare.predicate(), resolve(compare.left(), replacements), resolve(compare.right(), replacements));
        }
        if (instruction instanceof GlobalAddr globalAddr) {
            return new GlobalAddr(mappedTemp(globalAddr.result(), replacements), globalAddr.global());
        }
        if (instruction instanceof Load load) {
            return new Load(mappedTemp(load.result(), replacements), resolve(load.address(), replacements));
        }
        if (instruction instanceof Store store) {
            return new Store(resolve(store.value(), replacements), resolve(store.address(), replacements));
        }
        throw new IllegalStateException("unsupported instruction for inlining: " + instruction.getClass().getSimpleName());
    }

    private static Temp mappedTemp(Temp temp, Map<IRValue, IRValue> replacements) {
        IRValue mapped = replacements.get(temp);
        if (mapped instanceof Temp result) {
            return result;
        }
        throw new IllegalStateException("temp result was not mapped for inlining: " + temp.name());
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

    private static final class InlineContext {
        private int nextTemp;

        InlineContext(int nextTemp) {
            this.nextTemp = nextTemp;
        }

        Temp newTemp(Type type) {
            return new Temp(nextTemp++, type);
        }
    }

    private record InlineResult(List<Instruction> instructions, IRValue value) {
    }
}
