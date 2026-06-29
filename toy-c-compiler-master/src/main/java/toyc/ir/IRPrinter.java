package toyc.ir;

import java.util.StringJoiner;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.block.Module;
import toyc.ir.inst.Alloca;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.Branch;
import toyc.ir.inst.Call;
import toyc.ir.inst.Compare;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Load;
import toyc.ir.inst.LoadImm;
import toyc.ir.inst.Move;
import toyc.ir.inst.Phi;
import toyc.ir.inst.Return;
import toyc.ir.inst.Store;
import toyc.ir.inst.UnaryOp;
import toyc.ir.value.Constant;
import toyc.ir.value.GlobalVar;
import toyc.ir.value.Label;
import toyc.ir.value.LocalVar;
import toyc.ir.value.Temp;

public final class IRPrinter implements IRVisitor<Void, Integer> {
    private static final String INDENT = "  ";
    private final StringBuilder sb = new StringBuilder();

    private IRPrinter() {
    }

    public static String print(IRProgram program) {
        IRPrinter printer = new IRPrinter();
        program.accept(printer, 0);
        return printer.sb.toString();
    }

    private void emit(int depth, String text) {
        sb.append(INDENT.repeat(depth)).append(text).append('\n');
    }

    @Override
    public Void visitProgram(IRProgram node, Integer depth) {
        emit(depth, "IRProgram");
        node.module().accept(this, depth + 1);
        return null;
    }

    @Override
    public Void visitModule(Module node, Integer depth) {
        emit(depth, "Module");
        for (GlobalVar global : node.globals()) {
            global.accept(this, depth + 1);
        }
        for (Constant constant : node.globalConstants()) {
            constant.accept(this, depth + 1);
        }
        for (Function function : node.functions()) {
            function.accept(this, depth + 1);
        }
        return null;
    }

    @Override
    public Void visitFunction(Function node, Integer depth) {
        StringJoiner params = new StringJoiner(", ");
        for (LocalVar parameter : node.parameters()) {
            params.add(parameter.name());
        }
        emit(depth, "Function " + node.returnType() + " " + node.name() + "(" + params + ")");
        for (BasicBlock block : node.blocks()) {
            block.accept(this, depth + 1);
        }
        return null;
    }

    @Override
    public Void visitBasicBlock(BasicBlock node, Integer depth) {
        emit(depth, "Block " + node.label().name());
        node.allInstructions().forEach(instruction -> instruction.accept(this, depth + 1));
        return null;
    }

    @Override
    public Void visitConstant(Constant node, Integer depth) {
        emit(depth, "Constant " + node.name() + " = " + node.value());
        return null;
    }

    @Override
    public Void visitGlobalVar(GlobalVar node, Integer depth) {
        emit(depth, "GlobalVar " + node.name() + " = " + node.initialValue());
        return null;
    }

    @Override
    public Void visitLabel(Label node, Integer depth) {
        emit(depth, "Label " + node.name());
        return null;
    }

    @Override
    public Void visitLocalVar(LocalVar node, Integer depth) {
        emit(depth, "LocalVar " + node.name());
        return null;
    }

    @Override
    public Void visitTemp(Temp node, Integer depth) {
        emit(depth, "Temp " + node.name());
        return null;
    }

    @Override
    public Void visitAlloca(Alloca node, Integer depth) {
        emit(depth, node.result().name() + " = alloca");
        return null;
    }

    @Override
    public Void visitBinaryOp(BinaryOp node, Integer depth) {
        emit(depth, node.result().name() + " = " + node.op() + " " + node.left() + ", " + node.right());
        return null;
    }

    @Override
    public Void visitCompare(Compare node, Integer depth) {
        emit(depth, node.result().name() + " = cmp " + node.predicate() + " " + node.left() + ", " + node.right());
        return null;
    }

    @Override
    public Void visitUnaryOp(UnaryOp node, Integer depth) {
        emit(depth, node.result().name() + " = " + node.op() + " " + node.value());
        return null;
    }

    @Override
    public Void visitLoadImm(LoadImm node, Integer depth) {
        emit(depth, node.result().name() + " = imm " + node.constant().value());
        return null;
    }

    @Override
    public Void visitMove(Move node, Integer depth) {
        emit(depth, node.result().name() + " = move " + node.value());
        return null;
    }

    @Override
    public Void visitPhi(Phi node, Integer depth) {
        StringJoiner incoming = new StringJoiner(", ");
        for (Phi.Incoming value : node.incoming()) {
            incoming.add("[" + value.value() + ", " + value.predecessor().name() + "]");
        }
        emit(depth, node.result().name() + " = phi " + incoming);
        return null;
    }

    @Override
    public Void visitLoad(Load node, Integer depth) {
        emit(depth, node.result().name() + " = load " + node.address());
        return null;
    }

    @Override
    public Void visitStore(Store node, Integer depth) {
        emit(depth, "store " + node.value() + ", " + node.address());
        return null;
    }

    @Override
    public Void visitGlobalAddr(GlobalAddr node, Integer depth) {
        emit(depth, node.result().name() + " = globaladdr " + node.global().name());
        return null;
    }

    @Override
    public Void visitCall(Call node, Integer depth) {
        StringJoiner args = new StringJoiner(", ");
        for (var arg : node.args()) {
            args.add(arg.name());
        }
        String prefix = node.result() == null ? "" : node.result().name() + " = ";
        emit(depth, prefix + "call " + node.returnType() + " " + node.functionName() + "(" + args + ")");
        return null;
    }

    @Override
    public Void visitBranch(Branch node, Integer depth) {
        emit(depth, "br " + node.target().name());
        return null;
    }

    @Override
    public Void visitCondBranch(CondBranch node, Integer depth) {
        emit(depth, "cbr " + node.condition() + ", " + node.trueTarget().name() + ", " + node.falseTarget().name());
        return null;
    }

    @Override
    public Void visitReturn(Return node, Integer depth) {
        emit(depth, node.value() == null ? "ret" : "ret " + node.value());
        return null;
    }
}
