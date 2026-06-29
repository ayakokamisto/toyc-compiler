package toyc.opt;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.util.List;
import org.junit.jupiter.api.Test;
import toyc.common.Type;
import toyc.ir.IRProgram;
import toyc.ir.IRBuilder;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.block.Module;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.Branch;
import toyc.ir.inst.Call;
import toyc.ir.inst.Compare;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Load;
import toyc.ir.inst.LoadImm;
import toyc.ir.inst.Phi;
import toyc.ir.inst.Return;
import toyc.ir.inst.Store;
import toyc.ir.value.Constant;
import toyc.ir.value.GlobalVar;
import toyc.ir.value.Label;
import toyc.ir.value.LocalVar;
import toyc.ir.value.Temp;
import toyc.frontend.ParserFacade;
import toyc.semantic.SemanticAnalyzer;

class OptimizerTest {
    @Test
    void removesUnreachableBlocksAndInstructionsAfterTerminator() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        BasicBlock exit = new BasicBlock(new Label("exit"));
        BasicBlock unreachable = new BasicBlock(new Label("unreachable"));
        function.addBlock(exit);
        function.addBlock(unreachable);

        entry.addInstruction(new Branch(exit.label()));
        assertThrows(IllegalStateException.class, () -> entry.addInstruction(new LoadImm(new Temp(0, Type.INT), 42)));
        exit.addInstruction(new Return(new Temp(1, Type.INT)));
        unreachable.addInstruction(new Return(new Temp(2, Type.INT)));

        Module module = new Module();
        module.addFunction(function);
        Optimizer.optimize(new IRProgram(module));

        assertEquals(List.of(entry), function.blocks());
        assertEquals(1, entry.allInstructions().size());
        assertEquals(Return.class, entry.terminator().getClass());
    }

    @Test
    void foldsConstantConditionalBranches() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        BasicBlock thenBlock = new BasicBlock(new Label("then"));
        BasicBlock elseBlock = new BasicBlock(new Label("else"));
        function.addBlock(thenBlock);
        function.addBlock(elseBlock);

        entry.addInstruction(new CondBranch(Constant.of(0), thenBlock.label(), elseBlock.label()));
        thenBlock.addInstruction(new Return(new Temp(0, Type.INT)));
        elseBlock.addInstruction(new Return(new Temp(1, Type.INT)));

        optimize(function);

        assertEquals(List.of(entry), function.blocks());
        Return ret = (Return) entry.terminator();
        assertEquals("%t1", ret.value().name());
    }

    @Test
    void bypassesJumpOnlyBlocks() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        BasicBlock jumpOnly = new BasicBlock(new Label("jump"));
        BasicBlock exit = new BasicBlock(new Label("exit"));
        function.addBlock(jumpOnly);
        function.addBlock(exit);

        entry.addInstruction(new Branch(jumpOnly.label()));
        jumpOnly.addInstruction(new Branch(exit.label()));
        exit.addInstruction(new Return(new Temp(0, Type.INT)));

        optimize(function);

        assertEquals(List.of(entry), function.blocks());
        assertEquals(Return.class, entry.terminator().getClass());
    }

    @Test
    void mergesSinglePredecessorSingleSuccessorBlocks() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        BasicBlock middle = new BasicBlock(new Label("middle"));
        function.addBlock(middle);

        entry.addInstruction(new LoadImm(new Temp(0, Type.INT), 7));
        entry.addInstruction(new Branch(middle.label()));
        middle.addInstruction(new LoadImm(new Temp(1, Type.INT), 9));
        middle.addInstruction(new Return(new Temp(1, Type.INT)));

        optimize(function);

        assertEquals(List.of(entry), function.blocks());
        assertEquals(1, entry.allInstructions().size());
        assertEquals(Return.class, entry.terminator().getClass());
    }

    @Test
    void foldsLocalConstantsAndRemovesDeadTemps() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        Temp one = new Temp(0, Type.INT);
        Temp two = new Temp(1, Type.INT);
        Temp sum = new Temp(2, Type.INT);
        Temp copy = new Temp(3, Type.INT);
        Temp dead = new Temp(4, Type.INT);

        entry.addInstruction(new LoadImm(one, 1));
        entry.addInstruction(new LoadImm(two, 2));
        entry.addInstruction(new BinaryOp(sum, BinaryOp.Op.ADD, one, two));
        entry.addInstruction(new BinaryOp(copy, BinaryOp.Op.ADD, sum, Constant.of(0)));
        entry.addInstruction(new BinaryOp(dead, BinaryOp.Op.MUL, copy, Constant.of(9)));
        entry.addInstruction(new Return(copy));

        optimize(function);

        assertEquals(List.of(entry), function.blocks());
        assertEquals(1, entry.allInstructions().size());
        Return ret = (Return) entry.terminator();
        Constant value = (Constant) ret.value();
        assertEquals(3, value.value());
    }

    @Test
    void localFoldingFeedsControlFlowSimplification() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        BasicBlock thenBlock = new BasicBlock(new Label("then"));
        BasicBlock elseBlock = new BasicBlock(new Label("else"));
        function.addBlock(thenBlock);
        function.addBlock(elseBlock);
        Temp lhs = new Temp(0, Type.INT);
        Temp rhs = new Temp(1, Type.INT);
        Temp condition = new Temp(2, Type.INT);

        entry.addInstruction(new LoadImm(lhs, 4));
        entry.addInstruction(new LoadImm(rhs, 4));
        entry.addInstruction(new Compare(condition, Compare.Predicate.EQ, lhs, rhs));
        entry.addInstruction(new CondBranch(condition, thenBlock.label(), elseBlock.label()));
        thenBlock.addInstruction(new Return(Constant.of(7)));
        elseBlock.addInstruction(new Return(Constant.of(9)));

        optimize(function);

        assertEquals(List.of(entry), function.blocks());
        Return ret = (Return) entry.terminator();
        Constant value = (Constant) ret.value();
        assertEquals(7, value.value());
    }

    @Test
    void mem2RegPromotesBlockLocalStoreToLoad() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        LocalVar local = new LocalVar("x", 0, false);
        function.addLocal(local);
        Temp loaded = new Temp(0, Type.INT);

        entry.addInstruction(new Store(Constant.of(11), local));
        entry.addInstruction(new Load(loaded, local));
        entry.addInstruction(new Return(loaded));

        optimize(function);

        assertEquals(1, entry.allInstructions().size());
        Return ret = (Return) entry.terminator();
        Constant value = (Constant) ret.value();
        assertEquals(11, value.value());
    }

    @Test
    void mem2RegPreservesLocalValuesAcrossCalls() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        LocalVar local = new LocalVar("x", 0, false);
        function.addLocal(local);
        Temp callResult = new Temp(0, Type.INT);
        Temp loaded = new Temp(1, Type.INT);

        entry.addInstruction(new Store(Constant.of(11), local));
        entry.addInstruction(new Call(callResult, "mutate", Type.INT, List.of()));
        entry.addInstruction(new Load(loaded, local));
        entry.addInstruction(new Return(loaded));

        optimize(function);

        assertEquals(2, entry.allInstructions().size());
        Return ret = (Return) entry.terminator();
        Constant value = (Constant) ret.value();
        assertEquals(11, value.value());
    }

    @Test
    void removesDeadStoresToLocalVariables() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        LocalVar local = new LocalVar("x", 0, false);
        function.addLocal(local);

        entry.addInstruction(new Store(Constant.of(1), local));
        entry.addInstruction(new Store(Constant.of(2), local));
        entry.addInstruction(new Return(Constant.of(0)));

        optimize(function);

        assertEquals(1, entry.allInstructions().size());
        assertEquals(Return.class, entry.terminator().getClass());
    }

    @Test
    void promotesStoresThatFeedLoadsAcrossControlFlow() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        BasicBlock thenBlock = new BasicBlock(new Label("then"));
        BasicBlock endBlock = new BasicBlock(new Label("end"));
        function.addBlock(thenBlock);
        function.addBlock(endBlock);
        LocalVar local = new LocalVar("x", 0, false);
        function.addLocal(local);
        Temp loaded = new Temp(0, Type.INT);

        entry.addInstruction(new CondBranch(new Temp(1, Type.INT), thenBlock.label(), endBlock.label()));
        thenBlock.addInstruction(new Store(Constant.of(3), local));
        thenBlock.addInstruction(new Branch(endBlock.label()));
        endBlock.addInstruction(new Load(loaded, local));
        endBlock.addInstruction(new Return(loaded));

        optimize(function);

        boolean hasStore = function.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .anyMatch(Store.class::isInstance);
        boolean hasPhi = function.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .anyMatch(Phi.class::isInstance);
        assertEquals(false, hasStore);
        assertEquals(false, hasPhi);
    }

    @Test
    void lowersPhiInsertedForJoinBlock() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        BasicBlock thenBlock = new BasicBlock(new Label("then"));
        BasicBlock elseBlock = new BasicBlock(new Label("else"));
        BasicBlock joinBlock = new BasicBlock(new Label("join"));
        function.addBlock(thenBlock);
        function.addBlock(elseBlock);
        function.addBlock(joinBlock);
        LocalVar local = new LocalVar("x", 0, false);
        function.addLocal(local);
        Temp loaded = new Temp(0, Type.INT);

        entry.addInstruction(new CondBranch(new Temp(1, Type.INT), thenBlock.label(), elseBlock.label()));
        thenBlock.addInstruction(new Store(Constant.of(10), local));
        thenBlock.addInstruction(new Branch(joinBlock.label()));
        elseBlock.addInstruction(new Store(Constant.of(20), local));
        elseBlock.addInstruction(new Branch(joinBlock.label()));
        joinBlock.addInstruction(new Load(loaded, local));
        joinBlock.addInstruction(new Return(loaded));

        optimize(function);

        boolean hasPhi = function.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .anyMatch(Phi.class::isInstance);
        boolean hasStore = function.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .anyMatch(Store.class::isInstance);
        boolean hasLoad = function.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .anyMatch(Load.class::isInstance);
        assertEquals(false, hasPhi);
        assertEquals(false, hasStore);
        assertEquals(false, hasLoad);
    }

    @Test
    void inlinesSmallPureFunctions() {
        Module module = new Module();

        Function add1 = new Function("add1", Type.INT, List.of(new LocalVar("x", 0, true)), new Label("add1.entry"));
        Temp constOne = new Temp(0, Type.INT);
        Temp sum = new Temp(1, Type.INT);
        add1.entryBlock().addInstruction(new LoadImm(constOne, 1));
        add1.entryBlock().addInstruction(new BinaryOp(sum, BinaryOp.Op.ADD, add1.parameters().get(0), constOne));
        add1.entryBlock().addInstruction(new Return(sum));

        Function main = new Function("main", Type.INT, List.of(), new Label("entry"));
        Temp arg = new Temp(2, Type.INT);
        Temp callResult = new Temp(3, Type.INT);
        main.entryBlock().addInstruction(new LoadImm(arg, 41));
        main.entryBlock().addInstruction(new Call(callResult, "add1", Type.INT, List.of(arg)));
        main.entryBlock().addInstruction(new Return(callResult));

        module.addFunction(add1);
        module.addFunction(main);

        Optimizer.optimize(new IRProgram(module));

        boolean hasCall = main.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .anyMatch(Call.class::isInstance);
        assertEquals(false, hasCall);
        assertEquals(1, main.entryBlock().allInstructions().size());
        Return ret = (Return) main.entryBlock().terminator();
        Constant value = (Constant) ret.value();
        assertEquals(42, value.value());
    }

    @Test
    void inlinesSmallFunctionsGeneratedByIrBuilderAfterMem2Reg() {
        String source = """
                int add1(int x) {
                    return x + 1;
                }

                int main() {
                    return add1(41);
                }
                """;
        IRProgram program = build(source);
        Function main = program.module().mainFunction();

        Optimizer.optimize(program);

        boolean hasCall = main.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .anyMatch(Call.class::isInstance);
        assertEquals(false, hasCall);
        Return ret = (Return) main.entryBlock().terminator();
        Constant value = (Constant) ret.value();
        assertEquals(42, value.value());
    }

    @Test
    void givesEachInlineSiteFreshTemps() {
        Module module = new Module();

        Function add1 = new Function("add1", Type.INT, List.of(new LocalVar("x", 0, true)), new Label("add1.entry"));
        Temp constOne = new Temp(0, Type.INT);
        Temp sum = new Temp(1, Type.INT);
        add1.entryBlock().addInstruction(new LoadImm(constOne, 1));
        add1.entryBlock().addInstruction(new BinaryOp(sum, BinaryOp.Op.ADD, add1.parameters().get(0), constOne));
        add1.entryBlock().addInstruction(new Return(sum));

        Function main = new Function("main", Type.INT, List.of(), new Label("entry"));
        Temp first = new Temp(0, Type.INT);
        Temp second = new Temp(1, Type.INT);
        Temp total = new Temp(2, Type.INT);
        main.entryBlock().addInstruction(new Call(first, "add1", Type.INT, List.of(Constant.of(1))));
        main.entryBlock().addInstruction(new Call(second, "add1", Type.INT, List.of(Constant.of(2))));
        main.entryBlock().addInstruction(new BinaryOp(total, BinaryOp.Op.ADD, first, second));
        main.entryBlock().addInstruction(new Return(total));

        module.addFunction(add1);
        module.addFunction(main);

        Optimizer.optimize(new IRProgram(module));

        boolean hasCall = main.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .anyMatch(Call.class::isInstance);
        assertEquals(false, hasCall);
        Return ret = (Return) main.entryBlock().terminator();
        Constant value = (Constant) ret.value();
        assertEquals(5, value.value());
    }

    @Test
    void inlinesSmallVoidGlobalUpdateFunctions() {
        String source = """
                int g = 0;

                void add(int x) {
                    g = g + x;
                    return;
                }

                int main() {
                    add(7);
                    return g;
                }
                """;
        IRProgram program = build(source);
        Function main = program.module().mainFunction();

        Optimizer.optimize(program);

        boolean hasCall = main.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .anyMatch(Call.class::isInstance);
        assertEquals(false, hasCall);
    }

    @Test
    void doesNotInlineRecursiveMultiBlockFunctions() {
        String source = """
                int fib(int n) {
                    if (n <= 1) {
                        return n;
                    }
                    return fib(n - 1) + fib(n - 2);
                }

                int main() {
                    return fib(4);
                }
                """;
        IRProgram program = build(source);
        Function main = program.module().mainFunction();

        Optimizer.optimize(program);

        boolean hasCall = main.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .anyMatch(Call.class::isInstance);
        assertEquals(true, hasCall);
    }

    @Test
    void eliminatesRepeatedLocalExpressions() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        Temp x = new Temp(0, Type.INT);
        Temp first = new Temp(1, Type.INT);
        Temp second = new Temp(2, Type.INT);
        Temp total = new Temp(3, Type.INT);
        Constant three = Constant.of(3);

        entry.addInstruction(new Call(x, "unknown", Type.INT, List.of()));
        entry.addInstruction(new BinaryOp(first, BinaryOp.Op.MUL, x, three));
        entry.addInstruction(new BinaryOp(second, BinaryOp.Op.MUL, x, three));
        entry.addInstruction(new BinaryOp(total, BinaryOp.Op.ADD, first, second));
        entry.addInstruction(new Return(total));

        optimize(function);

        long multiplyCount = entry.instructions().stream()
                .filter(BinaryOp.class::isInstance)
                .map(BinaryOp.class::cast)
                .filter(binary -> binary.op() == BinaryOp.Op.MUL)
                .count();
        assertEquals(1, multiplyCount);
    }

    @Test
    void eliminatesRepeatedGlobalAddressesInBlock() {
        Module module = new Module();
        GlobalVar global = new GlobalVar("g", 0);
        module.addGlobal(global);
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        Temp firstAddress = new Temp(0, Type.INT);
        Temp secondAddress = new Temp(1, Type.INT);
        Temp loaded = new Temp(2, Type.INT);

        entry.addInstruction(new GlobalAddr(firstAddress, global));
        entry.addInstruction(new GlobalAddr(secondAddress, global));
        entry.addInstruction(new Load(loaded, secondAddress));
        entry.addInstruction(new Return(loaded));
        module.addFunction(function);

        Optimizer.optimize(new IRProgram(module));

        long globalAddrCount = entry.instructions().stream()
                .filter(GlobalAddr.class::isInstance)
                .count();
        assertEquals(1, globalAddrCount);
    }

    @Test
    void foldsLinearAddSubtractChains() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        Temp x = new Temp(0, Type.INT);
        Temp plusSix = new Temp(1, Type.INT);
        Temp minusFour = new Temp(2, Type.INT);
        Temp plusThirty = new Temp(3, Type.INT);

        entry.addInstruction(new Call(x, "unknown", Type.INT, List.of()));
        entry.addInstruction(new BinaryOp(plusSix, BinaryOp.Op.ADD, x, Constant.of(6)));
        entry.addInstruction(new BinaryOp(minusFour, BinaryOp.Op.SUB, plusSix, Constant.of(4)));
        entry.addInstruction(new BinaryOp(plusThirty, BinaryOp.Op.ADD, minusFour, Constant.of(30)));
        entry.addInstruction(new Return(plusThirty));

        optimize(function);

        Return ret = (Return) entry.terminator();
        BinaryOp folded = entry.instructions().stream()
                .filter(BinaryOp.class::isInstance)
                .map(BinaryOp.class::cast)
                .filter(binary -> binary.result() == ret.value())
                .findFirst()
                .orElseThrow();
        assertEquals(BinaryOp.Op.ADD, folded.op());
        assertEquals(x, folded.left());
        Constant offset = (Constant) folded.right();
        assertEquals(32, offset.value());
    }

    @Test
    void doesNotFoldRepeatedBaseAsLinearConstantOffset() {
        Function function = new Function("main", Type.INT, List.of(), new Label("entry"));
        BasicBlock entry = function.entryBlock();
        Temp x = new Temp(0, Type.INT);
        Temp doubled = new Temp(1, Type.INT);

        entry.addInstruction(new Call(x, "unknown", Type.INT, List.of()));
        entry.addInstruction(new BinaryOp(doubled, BinaryOp.Op.ADD, x, x));
        entry.addInstruction(new Return(doubled));

        optimize(function);

        BinaryOp add = entry.instructions().stream()
                .filter(BinaryOp.class::isInstance)
                .map(BinaryOp.class::cast)
                .filter(binary -> binary.result() == doubled)
                .findFirst()
                .orElseThrow();
        assertEquals(x, add.left());
        assertEquals(x, add.right());
    }

    @Test
    void removesFunctionsUnreachableAfterInlining() {
        Module module = new Module();
        Function dead = new Function("dead", Type.INT, List.of(), new Label("dead.entry"));
        dead.entryBlock().addInstruction(new Return(Constant.of(1)));
        Function main = new Function("main", Type.INT, List.of(), new Label("entry"));
        main.entryBlock().addInstruction(new Return(Constant.of(0)));
        module.addFunction(dead);
        module.addFunction(main);

        Optimizer.optimize(new IRProgram(module));

        assertEquals(List.of(main), module.functions());
    }

    @Test
    void promotesSimpleGlobalLoopStoreOutOfLoop() {
        String source = """
                int g = 0;

                void add(int x) {
                    g = g + x;
                    return;
                }

                int main() {
                    int i = 0;
                    while (i < 10) {
                        add(i % 3);
                        i = i + 1;
                    }
                    return g;
                }
                """;
        IRProgram program = build(source);

        Optimizer.optimize(program);

        Function main = program.module().mainFunction();
        long storeCount = main.blocks().stream()
                .flatMap(block -> block.allInstructions().stream())
                .filter(Store.class::isInstance)
                .count();
        assertEquals(1, storeCount);
    }

    private static void optimize(Function function) {
        Module module = new Module();
        module.addFunction(function);
        Optimizer.optimize(new IRProgram(module));
    }

    private static IRProgram build(String source) {
        var ast = ParserFacade.parse(source);
        var semanticResult = SemanticAnalyzer.analyze(ast);
        return IRBuilder.build(ast, semanticResult);
    }
}
