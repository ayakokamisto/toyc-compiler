package toyc.ir;

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

public abstract class IRTraversalVisitor<C> implements IRVisitor<Void, C> {
    @Override
    public Void visitProgram(IRProgram node, C context) {
        node.module().accept(this, context);
        return null;
    }

    @Override
    public Void visitModule(Module node, C context) {
        for (GlobalVar global : node.globals()) {
            global.accept(this, context);
        }
        for (Constant constant : node.globalConstants()) {
            constant.accept(this, context);
        }
        for (Function function : node.functions()) {
            function.accept(this, context);
        }
        return null;
    }

    @Override
    public Void visitFunction(Function node, C context) {
        for (LocalVar parameter : node.parameters()) {
            parameter.accept(this, context);
        }
        for (BasicBlock block : node.blocks()) {
            block.accept(this, context);
        }
        return null;
    }

    @Override
    public Void visitBasicBlock(BasicBlock node, C context) {
        node.label().accept(this, context);
        for (Instruction instruction : node.allInstructions()) {
            instruction.accept(this, context);
        }
        return null;
    }

    @Override
    public Void visitConstant(Constant node, C context) {
        return null;
    }

    @Override
    public Void visitGlobalVar(GlobalVar node, C context) {
        return null;
    }

    @Override
    public Void visitLabel(Label node, C context) {
        return null;
    }

    @Override
    public Void visitLocalVar(LocalVar node, C context) {
        return null;
    }

    @Override
    public Void visitTemp(Temp node, C context) {
        return null;
    }

    @Override
    public Void visitAlloca(Alloca node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitBinaryOp(BinaryOp node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitCompare(Compare node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitUnaryOp(UnaryOp node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitLoadImm(LoadImm node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitMove(Move node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitPhi(Phi node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitLoad(Load node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitStore(Store node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitGlobalAddr(GlobalAddr node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitCall(Call node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    @Override
    public Void visitBranch(Branch node, C context) {
        node.target().accept(this, context);
        return null;
    }

    @Override
    public Void visitCondBranch(CondBranch node, C context) {
        visitInstructionValues(node, context);
        node.trueTarget().accept(this, context);
        node.falseTarget().accept(this, context);
        return null;
    }

    @Override
    public Void visitReturn(Return node, C context) {
        visitInstructionValues(node, context);
        return null;
    }

    protected void visitInstructionValues(Instruction instruction, C context) {
        IRValue result = instruction.result();
        if (result != null) {
            result.accept(this, context);
        }
        for (IRValue operand : instruction.operands()) {
            operand.accept(this, context);
        }
    }
}
