package toyc.backend;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import toyc.ir.IRProgram;
import toyc.ir.IRVisitor;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.Alloca;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.Branch;
import toyc.ir.inst.Call;
import toyc.ir.inst.Compare;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Load;
import toyc.ir.inst.LoadImm;
import toyc.ir.inst.Move;
import toyc.ir.inst.Phi;
import toyc.ir.inst.Return;
import toyc.ir.inst.Store;
import toyc.ir.inst.UnaryOp;
import toyc.ir.value.Constant;
import toyc.ir.value.GlobalVar;
import toyc.ir.value.IRValue;
import toyc.ir.value.Label;
import toyc.ir.value.LocalVar;
import toyc.ir.value.Temp;
import toyc.semantic.SemanticResult;

public final class RiscVEmitter implements IRVisitor<Void, RiscVEmitter.Context> {
    private static final String[] ARG_REGS = {"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"};
    private static final String[] SAVED_ALLOCATABLE_REGS = {
        "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"
    };
    private static final String[] LEAF_EXTRA_ALLOCATABLE_REGS = {
        "t3", "t4", "t5", "a2", "a3", "a4", "a5", "a6", "a7"
    };
    private static final int REGISTER_ALLOCATION_MIN_SCORE = 6;
    private static final int WORD_SIZE = 4;

    private final StringBuilder sb = new StringBuilder();
    private final Map<IRValue, Integer> stackSlots = new IdentityHashMap<>();
    private final Map<IRValue, String> allocatedRegs = new IdentityHashMap<>();
    private final Map<String, Integer> savedRegOffsets = new LinkedHashMap<>();
    private final Map<IRValue, Boolean> nonNegativeValues = new IdentityHashMap<>();
    private final Set<GlobalVar> nonNegativeGlobals = Collections.newSetFromMap(new IdentityHashMap<>());
    private final List<String> activeAllocatableRegs = new ArrayList<>();
    private Function currentFunction;
    private int frameSize;

    private RiscVEmitter() {
    }

    public static String emit(IRProgram program, SemanticResult semanticResult) {
        RiscVEmitter emitter = new RiscVEmitter();
        program.accept(emitter, new Context());
        return emitter.peepholeAssembly(emitter.sb.toString());
    }

    @Override
    public Void visitProgram(IRProgram node, Context context) {
        node.module().accept(this, context);
        return null;
    }

    @Override
    public Void visitModule(toyc.ir.block.Module node, Context context) {
        if (node.mainFunction() == null) {
            throw new IllegalStateException("IR module does not contain main function");
        }

        if (!node.globals().isEmpty()) {
            emitLine(".data");
            for (GlobalVar global : node.globals()) {
                global.accept(this, context);
            }
        }

        emitLine(".text");
        for (Function function : node.functions()) {
            function.accept(this, context);
        }
        return null;
    }

    @Override
    public Void visitFunction(Function node, Context context) {
        currentFunction = node;
        allocateRegisters(node);
        analyzeNonNegativeValues(node);
        layoutFrame(node);

        emitLine(".globl " + asmSymbol(node.name()));
        emitLine(asmSymbol(node.name()) + ":");
        adjustStackPointer(-frameSize);
        storeWithOffset("ra", "sp", frameSize - WORD_SIZE);
        storeWithOffset("s0", "sp", frameSize - 2 * WORD_SIZE);
        for (Map.Entry<String, Integer> entry : savedRegOffsets.entrySet()) {
            storeWithOffset(entry.getKey(), "sp", entry.getValue());
        }
        emitLine("    mv s0, sp");
        spillIncomingParameters(node);

        for (BasicBlock block : node.blocks()) {
            block.accept(this, context);
        }

        currentFunction = null;
        stackSlots.clear();
        allocatedRegs.clear();
        savedRegOffsets.clear();
        nonNegativeValues.clear();
        nonNegativeGlobals.clear();
        activeAllocatableRegs.clear();
        frameSize = 0;
        return null;
    }

    @Override
    public Void visitBasicBlock(BasicBlock node, Context context) {
        emitLine(asmLabel(node.label()) + ":");
        for (Phi phi : node.phis()) {
            phi.accept(this, context);
        }
        List<Instruction> instructions = node.instructions();
        Instruction terminator = node.terminator();
        if (terminator instanceof CondBranch branch
                && !instructions.isEmpty()
                && instructions.get(instructions.size() - 1) instanceof Compare compare
                && branch.condition() == compare.result()) {
            for (int i = 0; i < instructions.size() - 1; i++) {
                instructions.get(i).accept(this, context);
            }
            emitCompareBranch(compare, branch.trueTarget(), branch.falseTarget());
            return null;
        }
        for (Instruction instruction : instructions) {
            instruction.accept(this, context);
        }
        if (terminator != null) {
            terminator.accept(this, context);
        }
        return null;
    }

    @Override
    public Void visitConstant(Constant node, Context context) {
        return null;
    }

    @Override
    public Void visitGlobalVar(GlobalVar node, Context context) {
        emitLine(".globl " + asmGlobal(node));
        emitLine(asmGlobal(node) + ":");
        emitLine("    .word " + node.initialValue());
        return null;
    }

    @Override
    public Void visitLabel(Label node, Context context) {
        return null;
    }

    @Override
    public Void visitLocalVar(LocalVar node, Context context) {
        return null;
    }

    @Override
    public Void visitTemp(Temp node, Context context) {
        return null;
    }

    @Override
    public Void visitAlloca(Alloca node, Context context) {
        return null;
    }

    @Override
    public Void visitBinaryOp(BinaryOp node, Context context) {
        if (emitBinaryImmediate(node)) {
            return null;
        }
        String target = resultRegister(node.result(), "t2");
        loadValue(node.left(), "t0");
        loadValue(node.right(), "t1");
        switch (node.op()) {
            case ADD -> emitLine("    add " + target + ", t0, t1");
            case SUB -> emitLine("    sub " + target + ", t0, t1");
            case MUL -> emitLine("    mul " + target + ", t0, t1");
            case DIV -> emitLine("    div " + target + ", t0, t1");
            case MOD -> emitLine("    rem " + target + ", t0, t1");
        }
        storeResult(node.result(), target);
        return null;
    }

    @Override
    public Void visitCompare(Compare node, Context context) {
        if (emitCompareImmediate(node)) {
            return null;
        }
        String target = resultRegister(node.result(), "t2");
        loadValue(node.left(), "t0");
        loadValue(node.right(), "t1");
        switch (node.predicate()) {
            case LT -> emitLine("    slt " + target + ", t0, t1");
            case GT -> emitLine("    slt " + target + ", t1, t0");
            case LE -> {
                emitLine("    slt " + target + ", t1, t0");
                emitLine("    xori " + target + ", " + target + ", 1");
            }
            case GE -> {
                emitLine("    slt " + target + ", t0, t1");
                emitLine("    xori " + target + ", " + target + ", 1");
            }
            case EQ -> {
                emitLine("    xor " + target + ", t0, t1");
                emitLine("    seqz " + target + ", " + target);
            }
            case NE -> {
                emitLine("    xor " + target + ", t0, t1");
                emitLine("    snez " + target + ", " + target);
            }
        }
        storeResult(node.result(), target);
        return null;
    }

    @Override
    public Void visitUnaryOp(UnaryOp node, Context context) {
        String target = resultRegister(node.result(), "t1");
        loadValue(node.value(), "t0");
        switch (node.op()) {
            case NEG -> emitLine("    neg " + target + ", t0");
            case NOT -> emitLine("    seqz " + target + ", t0");
        }
        storeResult(node.result(), target);
        return null;
    }

    @Override
    public Void visitLoadImm(LoadImm node, Context context) {
        String target = resultRegister(node.result(), "t0");
        emitLine("    li " + target + ", " + node.constant().value());
        storeResult(node.result(), target);
        return null;
    }

    @Override
    public Void visitMove(Move node, Context context) {
        String resultReg = allocatedRegs.get(node.result());
        String valueReg = allocatedRegs.get(node.value());
        if (resultReg != null && resultReg.equals(valueReg)) {
            return null;
        }
        loadValue(node.value(), "t0");
        storeValue(node.result(), "t0");
        return null;
    }

    @Override
    public Void visitPhi(Phi node, Context context) {
        throw unsupported("phi nodes before SSA lowering");
    }

    @Override
    public Void visitLoad(Load node, Context context) {
        String target = resultRegister(node.result(), "t0");
        if (node.address() instanceof LocalVar local) {
            loadValue(local, target);
        } else {
            loadAddress(node.address(), "t1");
            emitLine("    lw " + target + ", 0(t1)");
        }
        storeResult(node.result(), target);
        return null;
    }

    @Override
    public Void visitStore(Store node, Context context) {
        loadValue(node.value(), "t0");
        if (node.address() instanceof LocalVar local) {
            storeValue(local, "t0");
        } else {
            loadAddress(node.address(), "t1");
            emitLine("    sw t0, 0(t1)");
        }
        return null;
    }

    @Override
    public Void visitGlobalAddr(GlobalAddr node, Context context) {
        String target = resultRegister(node.result(), "t0");
        emitLine("    la " + target + ", " + asmGlobal(node.global()));
        storeResult(node.result(), target);
        return null;
    }

    @Override
    public Void visitCall(Call node, Context context) {
        int stackArgCount = Math.max(0, node.args().size() - ARG_REGS.length);
        int stackArgBytes = align(stackArgCount * WORD_SIZE, 16);

        for (int i = 0; i < Math.min(node.args().size(), ARG_REGS.length); i++) {
            loadValue(node.args().get(i), ARG_REGS[i]);
        }
        if (stackArgBytes > 0) {
            adjustStackPointer(-stackArgBytes);
            for (int i = ARG_REGS.length; i < node.args().size(); i++) {
                loadValue(node.args().get(i), "t0");
                storeWithOffset("t0", "sp", (i - ARG_REGS.length) * WORD_SIZE);
            }
        }

        emitLine("    call " + asmSymbol(node.functionName()));

        if (stackArgBytes > 0) {
            adjustStackPointer(stackArgBytes);
        }
        if (node.result() != null) {
            storeValue(node.result(), "a0");
        }
        return null;
    }

    @Override
    public Void visitBranch(Branch node, Context context) {
        emitLine("    j " + asmLabel(node.target()));
        return null;
    }

    @Override
    public Void visitCondBranch(CondBranch node, Context context) {
        loadValue(node.condition(), "t0");
        emitLine("    bnez t0, " + asmLabel(node.trueTarget()));
        emitLine("    j " + asmLabel(node.falseTarget()));
        return null;
    }

    @Override
    public Void visitReturn(Return node, Context context) {
        if (node.value() == null) {
            emitLine("    li a0, 0");
        } else {
            loadValue(node.value(), "a0");
        }
        emitEpilogue();
        return null;
    }

    private void layoutFrame(Function function) {
        stackSlots.clear();
        int nextOffset = 0;
        for (LocalVar local : function.locals().values()) {
            if (allocatedRegs.containsKey(local)) {
                continue;
            }
            stackSlots.put(local, nextOffset);
            nextOffset += WORD_SIZE;
        }
        for (BasicBlock block : function.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                IRValue result = instruction.result();
                if (result instanceof Temp && !allocatedRegs.containsKey(result) && !stackSlots.containsKey(result)) {
                    stackSlots.put(result, nextOffset);
                    nextOffset += WORD_SIZE;
                }
            }
        }
        int savedBytes = (2 + usedAllocatedRegisters().size()) * WORD_SIZE;
        frameSize = align(nextOffset + savedBytes, 16);
        savedRegOffsets.clear();
        int offset = frameSize - 3 * WORD_SIZE;
        for (String reg : usedAllocatedRegisters()) {
            savedRegOffsets.put(reg, offset);
            offset -= WORD_SIZE;
        }
    }

    private void spillIncomingParameters(Function function) {
        for (int i = 0; i < function.parameters().size(); i++) {
            LocalVar parameter = function.parameters().get(i);
            String allocated = allocatedRegs.get(parameter);
            if (i < ARG_REGS.length) {
                if (allocated != null) {
                    moveReg(allocated, ARG_REGS[i]);
                } else {
                    storeStack(parameter, ARG_REGS[i]);
                }
            } else {
                int callerOffset = frameSize + (i - ARG_REGS.length) * WORD_SIZE;
                loadWithOffset("t0", "s0", callerOffset);
                if (allocated != null) {
                    moveReg(allocated, "t0");
                } else {
                    storeStack(parameter, "t0");
                }
            }
        }
    }

    private void emitEpilogue() {
        emitLine("    mv sp, s0");
        loadWithOffset("ra", "sp", frameSize - WORD_SIZE);
        loadWithOffset("s0", "sp", frameSize - 2 * WORD_SIZE);
        for (Map.Entry<String, Integer> entry : savedRegOffsets.entrySet()) {
            loadWithOffset(entry.getKey(), "sp", entry.getValue());
        }
        adjustStackPointer(frameSize);
        emitLine("    ret");
    }

    private void loadValue(IRValue value, String targetReg) {
        if (value instanceof Constant constant) {
            emitLine("    li " + targetReg + ", " + constant.value());
            return;
        }
        if (value instanceof GlobalVar global) {
            emitLine("    la t6, " + asmGlobal(global));
            emitLine("    lw " + targetReg + ", 0(t6)");
            return;
        }
        String allocated = allocatedRegs.get(value);
        if (allocated != null) {
            moveReg(targetReg, allocated);
            return;
        }
        loadStack(targetReg, value);
    }

    private void storeValue(IRValue value, String sourceReg) {
        String allocated = allocatedRegs.get(value);
        if (allocated != null) {
            moveReg(allocated, sourceReg);
            return;
        }
        storeStack(value, sourceReg);
    }

    private String resultRegister(IRValue value, String scratchReg) {
        String allocated = allocatedRegs.get(value);
        return allocated == null ? scratchReg : allocated;
    }

    private void storeResult(IRValue value, String sourceReg) {
        if (!sourceReg.equals(allocatedRegs.get(value))) {
            storeValue(value, sourceReg);
        }
    }

    private void loadAddress(IRValue address, String targetReg) {
        if (address instanceof GlobalVar global) {
            emitLine("    la " + targetReg + ", " + asmGlobal(global));
            return;
        }
        if (address instanceof LocalVar local) {
            addImmediate(targetReg, "s0", stackOffset(local));
            return;
        }
        loadValue(address, targetReg);
    }

    private void loadStack(String targetReg, IRValue value) {
        loadWithOffset(targetReg, "s0", stackOffset(value));
    }

    private void storeStack(IRValue value, String sourceReg) {
        storeWithOffset(sourceReg, "s0", stackOffset(value));
    }

    private void loadWithOffset(String targetReg, String baseReg, int offset) {
        if (fitsSigned12(offset)) {
            emitLine("    lw " + targetReg + ", " + offset + "(" + baseReg + ")");
            return;
        }
        emitLine("    li t6, " + offset);
        emitLine("    add t6, " + baseReg + ", t6");
        emitLine("    lw " + targetReg + ", 0(t6)");
    }

    private void storeWithOffset(String sourceReg, String baseReg, int offset) {
        if (fitsSigned12(offset)) {
            emitLine("    sw " + sourceReg + ", " + offset + "(" + baseReg + ")");
            return;
        }
        emitLine("    li t6, " + offset);
        emitLine("    add t6, " + baseReg + ", t6");
        emitLine("    sw " + sourceReg + ", 0(t6)");
    }

    private void addImmediate(String targetReg, String sourceReg, int immediate) {
        if (fitsSigned12(immediate)) {
            emitLine("    addi " + targetReg + ", " + sourceReg + ", " + immediate);
            return;
        }
        emitLine("    li t6, " + immediate);
        emitLine("    add " + targetReg + ", " + sourceReg + ", t6");
    }

    private void adjustStackPointer(int amount) {
        if (fitsSigned12(amount)) {
            emitLine("    addi sp, sp, " + amount);
            return;
        }
        emitLine("    li t6, " + Math.abs(amount));
        emitLine(amount < 0 ? "    sub sp, sp, t6" : "    add sp, sp, t6");
    }

    private void allocateRegisters(Function function) {
        allocatedRegs.clear();
        activeAllocatableRegs.clear();
        activeAllocatableRegs.addAll(List.of(SAVED_ALLOCATABLE_REGS));
        if (isLeafFunction(function)) {
            activeAllocatableRegs.addAll(List.of(LEAF_EXTRA_ALLOCATABLE_REGS));
        }
        Map<IRValue, IRValue> parents = new IdentityHashMap<>();
        for (BasicBlock block : function.blocks()) {
            List<Instruction> instructions = block.instructions();
            for (int i = 1; i < instructions.size(); i++) {
                Instruction previous = instructions.get(i - 1);
                Instruction current = instructions.get(i);
                if (current instanceof Move move
                        && previous.result() == move.value()
                        && allocatableValue(previous.result())
                        && allocatableValue(move.result())) {
                    union(parents, previous.result(), move.result());
                }
            }
        }

        Map<IRValue, Integer> scores = new IdentityHashMap<>();
        Set<BasicBlock> loopBlocks = computeLoopLikeBlocks(function);
        for (BasicBlock block : function.blocks()) {
            int weight = loopBlocks.contains(block) ? 12 : 1;
            for (Instruction instruction : block.allInstructions()) {
                for (IRValue operand : instruction.operands()) {
                    if (allocatableValue(operand)) {
                        scores.merge(find(parents, operand), weight, Integer::sum);
                    }
                }
                IRValue result = instruction.result();
                if (allocatableValue(result)) {
                    scores.merge(find(parents, result), weight, Integer::sum);
                }
            }
        }

        List<Map.Entry<IRValue, Integer>> candidates = new ArrayList<>(scores.entrySet());
        candidates.sort(Comparator
                .<Map.Entry<IRValue, Integer>>comparingInt(Map.Entry::getValue)
                .reversed()
                .thenComparing(entry -> entry.getKey().name()));

        int regIndex = 0;
        for (Map.Entry<IRValue, Integer> entry : candidates) {
            if (regIndex >= activeAllocatableRegs.size()) {
                break;
            }
            if (entry.getValue() < REGISTER_ALLOCATION_MIN_SCORE) {
                continue;
            }
            String reg = activeAllocatableRegs.get(regIndex++);
            IRValue root = entry.getKey();
            for (IRValue value : scores.keySet()) {
                if (find(parents, value) == root) {
                    allocatedRegs.put(value, reg);
                }
            }
            for (BasicBlock block : function.blocks()) {
                for (Instruction instruction : block.allInstructions()) {
                    for (IRValue operand : instruction.operands()) {
                        if (allocatableValue(operand) && find(parents, operand) == root) {
                            allocatedRegs.put(operand, reg);
                        }
                    }
                    IRValue result = instruction.result();
                    if (allocatableValue(result) && find(parents, result) == root) {
                        allocatedRegs.put(result, reg);
                    }
                }
            }
        }
    }

    private IRValue find(Map<IRValue, IRValue> parents, IRValue value) {
        if (!allocatableValue(value)) {
            return value;
        }
        IRValue parent = parents.get(value);
        if (parent == null) {
            parents.put(value, value);
            return value;
        }
        if (parent == value) {
            return value;
        }
        IRValue root = find(parents, parent);
        parents.put(value, root);
        return root;
    }

    private void union(Map<IRValue, IRValue> parents, IRValue left, IRValue right) {
        IRValue leftRoot = find(parents, left);
        IRValue rightRoot = find(parents, right);
        if (leftRoot != rightRoot) {
            parents.put(rightRoot, leftRoot);
        }
    }

    private boolean allocatableValue(IRValue value) {
        if (value instanceof Temp) {
            return true;
        }
        return value instanceof LocalVar local && local.isParameter();
    }

    private List<String> usedAllocatedRegisters() {
        List<String> regs = new ArrayList<>();
        for (String reg : SAVED_ALLOCATABLE_REGS) {
            if (allocatedRegs.containsValue(reg)) {
                regs.add(reg);
            }
        }
        return regs;
    }

    private boolean isLeafFunction(Function function) {
        for (BasicBlock block : function.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                if (instruction instanceof Call) {
                    return false;
                }
            }
        }
        return true;
    }

    private Set<BasicBlock> computeLoopLikeBlocks(Function function) {
        Set<BasicBlock> loopBlocks = Collections.newSetFromMap(new IdentityHashMap<>());
        Map<Label, Integer> blockIndexes = new IdentityHashMap<>();
        List<BasicBlock> blocks = function.blocks();
        for (int i = 0; i < blocks.size(); i++) {
            blockIndexes.put(blocks.get(i).label(), i);
            String name = blocks.get(i).label().name();
            if (name.contains("while.cond") || name.contains("while.body")
                    || name.contains("land.rhs") || name.contains("lor.rhs")) {
                loopBlocks.add(blocks.get(i));
            }
        }
        for (int i = 0; i < blocks.size(); i++) {
            for (Label target : branchTargets(blocks.get(i).terminator())) {
                Integer targetIndex = blockIndexes.get(target);
                if (targetIndex != null && targetIndex <= i) {
                    for (int j = targetIndex; j <= i; j++) {
                        loopBlocks.add(blocks.get(j));
                    }
                }
            }
        }
        return loopBlocks;
    }

    private List<Label> branchTargets(Instruction terminator) {
        if (terminator instanceof Branch branch) {
            return List.of(branch.target());
        }
        if (terminator instanceof CondBranch branch) {
            return List.of(branch.trueTarget(), branch.falseTarget());
        }
        return List.of();
    }

    private boolean emitBinaryImmediate(BinaryOp node) {
        if (node.right() instanceof Constant constant) {
            int imm = constant.value();
            switch (node.op()) {
                case ADD -> {
                    String target = resultRegister(node.result(), "t2");
                    loadValue(node.left(), "t0");
                    addImmediate(target, "t0", imm);
                    storeResult(node.result(), target);
                    return true;
                }
                case SUB -> {
                    String target = resultRegister(node.result(), "t2");
                    loadValue(node.left(), "t0");
                    addImmediate(target, "t0", -imm);
                    storeResult(node.result(), target);
                    return true;
                }
                case MUL -> {
                    Integer shift = powerOfTwoShift(Math.abs(imm));
                    if (shift != null) {
                        String target = resultRegister(node.result(), "t2");
                        loadValue(node.left(), "t0");
                        emitLine("    slli " + target + ", t0, " + shift);
                        if (imm < 0) {
                            emitLine("    neg " + target + ", " + target);
                        }
                        storeResult(node.result(), target);
                        return true;
                    }
                }
                case DIV -> {
                    Integer shift = powerOfTwoShift(imm);
                    if (shift != null && isNonNegative(node.left())) {
                        String target = resultRegister(node.result(), "t2");
                        loadValue(node.left(), "t0");
                        emitLine("    srli " + target + ", t0, " + shift);
                        storeResult(node.result(), target);
                        return true;
                    }
                    if (imm > 1 && isNonNegative(node.left())) {
                        String target = resultRegister(node.result(), "t2");
                        loadValue(node.left(), "t0");
                        emitUnsignedDivConstant(target, "t0", imm);
                        storeResult(node.result(), target);
                        return true;
                    }
                }
                case MOD -> {
                    Integer shift = powerOfTwoShift(imm);
                    if (shift != null && isNonNegative(node.left())) {
                        String target = resultRegister(node.result(), "t2");
                        loadValue(node.left(), "t0");
                        emitUnsignedPowerOfTwoRemainder(target, "t0", shift);
                        storeResult(node.result(), target);
                        return true;
                    }
                    if (imm > 1 && isNonNegative(node.left())) {
                        String target = resultRegister(node.result(), "t2");
                        loadValue(node.left(), "t0");
                        emitUnsignedRemainderConstant(target, "t0", imm);
                        storeResult(node.result(), target);
                        return true;
                    }
                }
            }
        }
        if (node.left() instanceof Constant constant && node.op() == BinaryOp.Op.ADD) {
            String target = resultRegister(node.result(), "t2");
            loadValue(node.right(), "t0");
            addImmediate(target, "t0", constant.value());
            storeResult(node.result(), target);
            return true;
        }
        if (node.left() instanceof Constant constant && node.op() == BinaryOp.Op.MUL) {
            Integer shift = powerOfTwoShift(Math.abs(constant.value()));
            if (shift != null) {
                String target = resultRegister(node.result(), "t2");
                loadValue(node.right(), "t0");
                emitLine("    slli " + target + ", t0, " + shift);
                if (constant.value() < 0) {
                    emitLine("    neg " + target + ", " + target);
                }
                storeResult(node.result(), target);
                return true;
            }
        }
        return false;
    }

    private boolean emitCompareImmediate(Compare node) {
        if (!(node.right() instanceof Constant constant)) {
            return false;
        }
        int imm = constant.value();
        String target = resultRegister(node.result(), "t2");
        loadValue(node.left(), "t0");
        switch (node.predicate()) {
            case LT -> {
                if (!fitsSigned12(imm)) {
                    return false;
                }
                emitLine("    slti " + target + ", t0, " + imm);
            }
            case GE -> {
                if (!fitsSigned12(imm)) {
                    return false;
                }
                emitLine("    slti " + target + ", t0, " + imm);
                emitLine("    xori " + target + ", " + target + ", 1");
            }
            case LE -> {
                if (imm == Integer.MAX_VALUE) {
                    emitLine("    li " + target + ", 1");
                } else if (fitsSigned12(imm + 1)) {
                    emitLine("    slti " + target + ", t0, " + (imm + 1));
                } else {
                    return false;
                }
            }
            case GT -> {
                if (imm == Integer.MAX_VALUE) {
                    emitLine("    li " + target + ", 0");
                } else if (fitsSigned12(imm + 1)) {
                    emitLine("    slti " + target + ", t0, " + (imm + 1));
                    emitLine("    xori " + target + ", " + target + ", 1");
                } else {
                    return false;
                }
            }
            case EQ -> {
                addImmediate(target, "t0", -imm);
                emitLine("    seqz " + target + ", " + target);
            }
            case NE -> {
                addImmediate(target, "t0", -imm);
                emitLine("    snez " + target + ", " + target);
            }
        }
        storeResult(node.result(), target);
        return true;
    }

    private void emitCompareBranch(Compare compare, Label trueTarget, Label falseTarget) {
        loadValue(compare.left(), "t0");
        if (compare.right() instanceof Constant constant && constant.value() == 0) {
            switch (compare.predicate()) {
                case LT -> emitLine("    bltz t0, " + asmLabel(trueTarget));
                case GT -> emitLine("    bgtz t0, " + asmLabel(trueTarget));
                case LE -> emitLine("    blez t0, " + asmLabel(trueTarget));
                case GE -> emitLine("    bgez t0, " + asmLabel(trueTarget));
                case EQ -> emitLine("    beqz t0, " + asmLabel(trueTarget));
                case NE -> emitLine("    bnez t0, " + asmLabel(trueTarget));
            }
            emitLine("    j " + asmLabel(falseTarget));
            return;
        }
        loadValue(compare.right(), "t1");
        switch (compare.predicate()) {
            case LT -> emitLine("    blt t0, t1, " + asmLabel(trueTarget));
            case GT -> emitLine("    blt t1, t0, " + asmLabel(trueTarget));
            case LE -> emitLine("    bge t1, t0, " + asmLabel(trueTarget));
            case GE -> emitLine("    bge t0, t1, " + asmLabel(trueTarget));
            case EQ -> emitLine("    beq t0, t1, " + asmLabel(trueTarget));
            case NE -> emitLine("    bne t0, t1, " + asmLabel(trueTarget));
        }
        emitLine("    j " + asmLabel(falseTarget));
    }

    private void analyzeNonNegativeValues(Function function) {
        nonNegativeValues.clear();
        nonNegativeGlobals.clear();
        Set<GlobalVar> globalCandidates = collectInitiallyNonNegativeGlobals(function);
        nonNegativeGlobals.addAll(globalCandidates);

        boolean changed;
        do {
            nonNegativeValues.clear();
            Map<IRValue, GlobalVar> globalAddresses = new IdentityHashMap<>();
            boolean valueChanged;
            do {
                valueChanged = false;
                for (BasicBlock block : function.blocks()) {
                    for (Instruction instruction : block.allInstructions()) {
                        recordGlobalAddress(instruction, globalAddresses);
                        IRValue result = instruction.result();
                        if (result != null && !isNonNegative(result)
                                && producesNonNegative(instruction, globalAddresses)) {
                            nonNegativeValues.put(result, Boolean.TRUE);
                            valueChanged = true;
                        }
                    }
                }
            } while (valueChanged);

            changed = false;
            for (BasicBlock block : function.blocks()) {
                for (Instruction instruction : block.allInstructions()) {
                    if (instruction instanceof Store store) {
                        GlobalVar global = globalForAddress(store.address(), globalAddresses);
                        if (global != null && nonNegativeGlobals.contains(global)
                                && !isNonNegative(store.value())) {
                            nonNegativeGlobals.remove(global);
                            changed = true;
                        }
                    }
                    if (instruction instanceof Call && !nonNegativeGlobals.isEmpty()) {
                        nonNegativeGlobals.clear();
                        changed = true;
                    }
                }
            }
        } while (changed);
    }

    private Set<GlobalVar> collectInitiallyNonNegativeGlobals(Function function) {
        Set<GlobalVar> globals = Collections.newSetFromMap(new IdentityHashMap<>());
        for (BasicBlock block : function.blocks()) {
            for (Instruction instruction : block.allInstructions()) {
                if (instruction instanceof GlobalAddr globalAddr && globalAddr.global().initialValue() >= 0) {
                    globals.add(globalAddr.global());
                }
                for (IRValue operand : instruction.operands()) {
                    if (operand instanceof GlobalVar global && global.initialValue() >= 0) {
                        globals.add(global);
                    }
                }
            }
        }
        return globals;
    }

    private void recordGlobalAddress(Instruction instruction, Map<IRValue, GlobalVar> globalAddresses) {
        if (instruction instanceof GlobalAddr globalAddr) {
            globalAddresses.put(globalAddr.result(), globalAddr.global());
            return;
        }
        if (instruction instanceof Move move) {
            GlobalVar global = globalForAddress(move.value(), globalAddresses);
            if (global != null) {
                globalAddresses.put(move.result(), global);
            }
        }
    }

    private GlobalVar globalForAddress(IRValue address, Map<IRValue, GlobalVar> globalAddresses) {
        if (address instanceof GlobalVar global) {
            return global;
        }
        return globalAddresses.get(address);
    }

    private boolean producesNonNegative(Instruction instruction, Map<IRValue, GlobalVar> globalAddresses) {
        if (instruction instanceof LoadImm loadImm) {
            return loadImm.constant().value() >= 0;
        }
        if (instruction instanceof Move move) {
            return isNonNegative(move.value());
        }
        if (instruction instanceof Compare) {
            return true;
        }
        if (instruction instanceof UnaryOp unary && unary.op() == UnaryOp.Op.NOT) {
            return true;
        }
        if (instruction instanceof Load load) {
            GlobalVar global = globalForAddress(load.address(), globalAddresses);
            return global != null && nonNegativeGlobals.contains(global);
        }
        if (instruction instanceof BinaryOp binary) {
            return switch (binary.op()) {
                case ADD, MUL -> isNonNegative(binary.left()) && isNonNegative(binary.right());
                case DIV, MOD -> isNonNegative(binary.left()) && positiveConstant(binary.right());
                case SUB -> false;
            };
        }
        return false;
    }

    private boolean isNonNegative(IRValue value) {
        if (value instanceof Constant constant) {
            return constant.value() >= 0;
        }
        if (value instanceof GlobalVar global) {
            return nonNegativeGlobals.contains(global);
        }
        return Boolean.TRUE.equals(nonNegativeValues.get(value));
    }

    private boolean positiveConstant(IRValue value) {
        return value instanceof Constant constant && constant.value() > 0;
    }

    private Integer powerOfTwoShift(int value) {
        if (value <= 0 || (value & (value - 1)) != 0) {
            return null;
        }
        return Integer.numberOfTrailingZeros(value);
    }

    private void emitUnsignedDivConstant(String targetReg, String sourceReg, int divisor) {
        MagicUnsigned magic = magicUnsigned(divisor);
        emitLoadUnsigned32("t1", magic.multiplier());
        emitLine("    mulhu " + targetReg + ", " + sourceReg + ", t1");
        if (magic.addIndicator()) {
            emitLine("    sub t1, " + sourceReg + ", " + targetReg);
            emitLine("    srli t1, t1, 1");
            emitLine("    add " + targetReg + ", " + targetReg + ", t1");
            if (magic.shift() > 1) {
                emitLine("    srli " + targetReg + ", " + targetReg + ", " + (magic.shift() - 1));
            }
        } else if (magic.shift() > 0) {
            emitLine("    srli " + targetReg + ", " + targetReg + ", " + magic.shift());
        }
    }

    private void emitUnsignedRemainderConstant(String targetReg, String sourceReg, int divisor) {
        emitUnsignedDivConstant("t2", sourceReg, divisor);
        emitLoadUnsigned32("t1", divisor);
        emitLine("    mul t2, t2, t1");
        emitLine("    sub " + targetReg + ", " + sourceReg + ", t2");
    }

    private void emitUnsignedPowerOfTwoRemainder(String targetReg, String sourceReg, int shift) {
        int mask = (1 << shift) - 1;
        if (fitsSigned12(mask)) {
            emitLine("    andi " + targetReg + ", " + sourceReg + ", " + mask);
            return;
        }
        int clearHighShift = 32 - shift;
        emitLine("    slli " + targetReg + ", " + sourceReg + ", " + clearHighShift);
        emitLine("    srli " + targetReg + ", " + targetReg + ", " + clearHighShift);
    }

    private void emitLoadUnsigned32(String targetReg, long value) {
        emitLine("    li " + targetReg + ", " + (int) value);
    }

    private MagicUnsigned magicUnsigned(int divisor) {
        long unsignedDivisor = Integer.toUnsignedLong(divisor);
        long two31 = 1L << 31;
        long nc = 0xffffffffL - Long.remainderUnsigned(0xffffffffL, unsignedDivisor);
        int p = 31;
        long q1 = Long.divideUnsigned(two31, nc);
        long r1 = two31 - q1 * nc;
        long q2 = Long.divideUnsigned(two31, unsignedDivisor);
        long r2 = two31 - q2 * unsignedDivisor;
        long delta;
        do {
            p++;
            q1 *= 2;
            r1 *= 2;
            if (Long.compareUnsigned(r1, nc) >= 0) {
                q1++;
                r1 -= nc;
            }
            q2 *= 2;
            r2 *= 2;
            if (Long.compareUnsigned(r2, unsignedDivisor) >= 0) {
                q2++;
                r2 -= unsignedDivisor;
            }
            delta = unsignedDivisor - 1 - r2;
        } while (Long.compareUnsigned(q1, delta) < 0
                || (q1 == delta && r1 == 0));

        long multiplier = q2 + 1;
        return new MagicUnsigned(multiplier & 0xffffffffL, p - 32, multiplier > 0xffffffffL);
    }

    private void moveReg(String targetReg, String sourceReg) {
        if (!targetReg.equals(sourceReg)) {
            emitLine("    mv " + targetReg + ", " + sourceReg);
        }
    }

    private String peepholeAssembly(String asm) {
        String[] lines = asm.split("\\R", -1);
        List<String> kept = new ArrayList<>(lines.length);
        for (String line : lines) {
            if (line.startsWith("    mv ")) {
                String operands = line.substring("    mv ".length());
                int comma = operands.indexOf(',');
                if (comma > 0) {
                    String left = operands.substring(0, comma).trim();
                    String right = operands.substring(comma + 1).trim();
                    if (left.equals(right)) {
                        continue;
                    }
                }
            }
            kept.add(line);
        }
        return String.join("\n", kept);
    }

    private int stackOffset(IRValue value) {
        Integer offset = stackSlots.get(value);
        if (offset == null) {
            throw unsupported("stack slot for " + value.name());
        }
        return offset;
    }

    private String asmLabel(Label label) {
        return asmSymbol(label.name());
    }

    private String asmGlobal(GlobalVar global) {
        return asmSymbol(global.symbolName());
    }

    private String asmSymbol(String symbol) {
        return symbol.replace('%', '_').replace('@', '_').replace('.', '_');
    }

    private int align(int value, int alignment) {
        return ((value + alignment - 1) / alignment) * alignment;
    }

    private boolean fitsSigned12(int value) {
        return value >= -2048 && value <= 2047;
    }

    private void emitLine(String line) {
        sb.append(line).append('\n');
    }

    private UnsupportedOperationException unsupported(String feature) {
        return new UnsupportedOperationException("RISC-V emitter does not yet support " + feature);
    }

    private record MagicUnsigned(long multiplier, int shift, boolean addIndicator) {
    }

    static final class Context {
    }
}
