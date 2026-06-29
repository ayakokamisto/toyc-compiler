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

public interface IRVisitor<R, C> {
    R visitProgram(IRProgram node, C context);
    R visitModule(Module node, C context);
    R visitFunction(Function node, C context);
    R visitBasicBlock(BasicBlock node, C context);

    R visitConstant(Constant node, C context);
    R visitGlobalVar(GlobalVar node, C context);
    R visitLabel(Label node, C context);
    R visitLocalVar(LocalVar node, C context);
    R visitTemp(Temp node, C context);

    R visitAlloca(Alloca node, C context);
    R visitBinaryOp(BinaryOp node, C context);
    R visitCompare(Compare node, C context);
    R visitUnaryOp(UnaryOp node, C context);
    R visitLoadImm(LoadImm node, C context);
    R visitMove(Move node, C context);
    R visitPhi(Phi node, C context);
    R visitLoad(Load node, C context);
    R visitStore(Store node, C context);
    R visitGlobalAddr(GlobalAddr node, C context);
    R visitCall(Call node, C context);
    R visitBranch(Branch node, C context);
    R visitCondBranch(CondBranch node, C context);
    R visitReturn(Return node, C context);
}
