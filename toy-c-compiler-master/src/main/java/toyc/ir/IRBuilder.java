package toyc.ir;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.OptionalInt;
import toyc.common.Type;
import toyc.frontend.ast.ASTNode;
import toyc.frontend.ast.Decl;
import toyc.frontend.ast.Expr;
import toyc.frontend.ast.FuncDef;
import toyc.frontend.ast.Program;
import toyc.frontend.ast.Stmt;
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
import toyc.ir.inst.Return;
import toyc.ir.inst.Store;
import toyc.ir.inst.UnaryOp;
import toyc.ir.value.Constant;
import toyc.ir.value.GlobalVar;
import toyc.ir.value.IRValue;
import toyc.ir.value.Label;
import toyc.ir.value.LocalVar;
import toyc.ir.value.Temp;
import toyc.semantic.FunctionSymbol;
import toyc.semantic.SemanticResult;
import toyc.semantic.Symbol;

public final class IRBuilder {
    private final Program program;
    private final SemanticResult semanticResult;
    private final Module module = new Module();
    private final Map<Symbol, IRValue> symbolValues = new IdentityHashMap<>();
    private final Deque<LoopTarget> loopTargets = new ArrayDeque<>();
    private Function currentFunction;
    private BasicBlock currentBlock;
    private int tempCount;
    private int labelCount;
    private int localCount;

    private IRBuilder(Program program, SemanticResult semanticResult) {
        this.program = program;
        this.semanticResult = semanticResult;
    }

    public static IRProgram build(Program program, SemanticResult semanticResult) {
        return new IRProgram(new IRBuilder(program, semanticResult).buildModule());
    }

    public Module buildModule() {
        for (ASTNode def : program.defs()) {
            if (def instanceof Decl decl) {
                emitGlobalDecl(decl);
            }
        }
        for (ASTNode def : program.defs()) {
            if (def instanceof FuncDef funcDef) {
                emitFunction(funcDef);
            }
        }
        return module;
    }

    public Temp newTemp(Type type) {
        return new Temp(tempCount++, type);
    }

    public Label newLabel(String hint) {
        return new Label(hint + "." + labelCount++);
    }

    public LocalVar newLocal(String sourceName, boolean parameter) {
        LocalVar local = new LocalVar(sourceName, localCount++, parameter);
        if (currentFunction != null) {
            currentFunction.addLocal(local);
        }
        return local;
    }

    public BasicBlock appendBlock(String hint) {
        BasicBlock block = new BasicBlock(newLabel(hint));
        currentFunction.addBlock(block);
        return block;
    }

    public void setCurrentBlock(BasicBlock block) {
        currentBlock = block;
    }

    public void emit(Instruction instruction) {
        currentBlock.addInstruction(instruction);
    }

    public void emitBranchIfOpen(Label target) {
        if (!currentBlock.isTerminated()) {
            emit(new Branch(target));
        }
    }

    public IRValue emitExpr(Expr expr) {
        if (expr instanceof Expr.IntLiteral literal) {
            return emitLoadImm(literal.value());
        }
        if (expr instanceof Expr.Id id) {
            return emitId(id);
        }
        if (expr instanceof Expr.Unary unary) {
            return emitUnary(unary);
        }
        if (expr instanceof Expr.Binary binary) {
            return emitBinary(binary);
        }
        if (expr instanceof Expr.FuncCall call) {
            return emitCall(call);
        }
        throw new IllegalArgumentException("unsupported expression: " + expr.getClass().getSimpleName());
    }

    public void emitStmt(Stmt stmt) {
        if (stmt instanceof Decl decl) {
            emitLocalDecl(decl);
        } else if (stmt instanceof Stmt.Block block) {
            emitBlock(block);
        } else if (stmt instanceof Stmt.Empty) {
            return;
        } else if (stmt instanceof Stmt.ExprStmt exprStmt) {
            emitExpr(exprStmt.expr());
        } else if (stmt instanceof Stmt.Assign assign) {
            emitAssign(assign);
        } else if (stmt instanceof Stmt.If ifStmt) {
            emitIf(ifStmt);
        } else if (stmt instanceof Stmt.While whileStmt) {
            emitWhile(whileStmt);
        } else if (stmt instanceof Stmt.Break) {
            emitBreak();
        } else if (stmt instanceof Stmt.Continue) {
            emitContinue();
        } else if (stmt instanceof Stmt.Return ret) {
            emitReturn(ret);
        } else {
            throw new IllegalArgumentException("unsupported statement: " + stmt.getClass().getSimpleName());
        }
    }

    private void emitGlobalDecl(Decl decl) {
        Symbol symbol = semanticResult.symbolOf(decl);
        int initialValue = constValueOrZero(decl instanceof Decl.VarDecl varDecl ? varDecl.init()
                : ((Decl.ConstDecl) decl).init());
        if (decl instanceof Decl.ConstDecl) {
            Constant constant = Constant.named(symbol.name(), initialValue);
            module.addGlobalConstant(constant);
            symbolValues.put(symbol, constant);
        } else {
            GlobalVar global = new GlobalVar(symbol.name(), initialValue);
            module.addGlobal(global);
            symbolValues.put(symbol, global);
        }
    }

    private void emitFunction(FuncDef funcDef) {
        tempCount = 0;
        localCount = 0;
        List<LocalVar> parameters = new ArrayList<>();
        for (FuncDef.Param param : funcDef.params()) {
            LocalVar local = newLocal(param.id(), true);
            parameters.add(local);
            symbolValues.put(semanticResult.symbolOf(param), local);
        }
        currentFunction = new Function(funcDef.id(), toCommonType(funcDef.returnType()), parameters, newLabel(funcDef.id() + ".entry"));
        module.addFunction(currentFunction);
        currentBlock = currentFunction.entryBlock();
        for (LocalVar parameter : parameters) {
            emit(new Alloca(parameter));
        }
        emitBlock(funcDef.body());
        if (!currentBlock.isTerminated()) {
            IRValue defaultValue = currentFunction.returnType() == Type.VOID ? null : emitLoadImm(0);
            emit(new Return(defaultValue));
        }
        currentFunction = null;
        currentBlock = null;
    }

    private void emitBlock(Stmt.Block block) {
        for (Stmt stmt : block.stmts()) {
            if (currentBlock.isTerminated()) {
                break;
            }
            emitStmt(stmt);
        }
    }

    private void emitLocalDecl(Decl decl) {
        Symbol symbol = semanticResult.symbolOf(decl);
        if (decl instanceof Decl.ConstDecl constDecl) {
            int value = constValueOrZero(constDecl.init());
            symbolValues.put(symbol, Constant.named(symbol.name(), value));
            return;
        }
        Decl.VarDecl varDecl = (Decl.VarDecl) decl;
        LocalVar local = newLocal(varDecl.id(), false);
        symbolValues.put(symbol, local);
        emit(new Alloca(local));
        emit(new Store(emitExpr(varDecl.init()), local));
    }

    private void emitAssign(Stmt.Assign assign) {
        IRValue address = addressOf(semanticResult.symbolOf(assign));
        emit(new Store(emitExpr(assign.expr()), address));
    }

    private void emitIf(Stmt.If ifStmt) {
        BasicBlock thenBlock = appendBlock("if.then");
        BasicBlock elseBlock = ifStmt.elseStmt() == null ? null : appendBlock("if.else");
        BasicBlock endBlock = appendBlock("if.end");
        emitCondExpr(ifStmt.cond(), thenBlock.label(), elseBlock == null ? endBlock.label() : elseBlock.label());

        setCurrentBlock(thenBlock);
        emitStmt(ifStmt.thenStmt());
        emitBranchIfOpen(endBlock.label());

        if (elseBlock != null) {
            setCurrentBlock(elseBlock);
            emitStmt(ifStmt.elseStmt());
            emitBranchIfOpen(endBlock.label());
        }

        setCurrentBlock(endBlock);
    }

    private void emitWhile(Stmt.While whileStmt) {
        BasicBlock condBlock = appendBlock("while.cond");
        BasicBlock bodyBlock = appendBlock("while.body");
        BasicBlock endBlock = appendBlock("while.end");
        emit(new Branch(condBlock.label()));

        setCurrentBlock(condBlock);
        emitCondExpr(whileStmt.cond(), bodyBlock.label(), endBlock.label());

        loopTargets.push(new LoopTarget(condBlock.label(), endBlock.label()));
        setCurrentBlock(bodyBlock);
        emitStmt(whileStmt.body());
        emitBranchIfOpen(condBlock.label());
        loopTargets.pop();

        setCurrentBlock(endBlock);
    }

    private void emitBreak() {
        emit(new Branch(loopTargets.peek().breakTarget()));
    }

    private void emitContinue() {
        emit(new Branch(loopTargets.peek().continueTarget()));
    }

    private void emitReturn(Stmt.Return ret) {
        emit(new Return(ret.expr() == null ? null : emitExpr(ret.expr())));
    }

    private IRValue emitId(Expr.Id id) {
        IRValue value = symbolValues.get(semanticResult.symbolOf(id));
        if (value instanceof Constant constant) {
            return emitLoadImm(constant.value());
        }
        Temp result = newTemp(Type.INT);
        emit(new Load(result, addressOf(value)));
        return result;
    }

    private IRValue emitUnary(Expr.Unary unary) {
        IRValue value = emitExpr(unary.expr());
        return switch (unary.op()) {
            case PLUS -> value;
            case MINUS -> {
                Temp result = newTemp(Type.INT);
                emit(new UnaryOp(result, UnaryOp.Op.NEG, value));
                yield result;
            }
            case NOT -> {
                Temp result = newTemp(Type.INT);
                emit(new UnaryOp(result, UnaryOp.Op.NOT, value));
                yield result;
            }
        };
    }

    private IRValue emitBinary(Expr.Binary binary) {
        if (binary.op() == Expr.BinaryOp.AND) {
            return emitShortCircuitAnd(binary);
        }
        if (binary.op() == Expr.BinaryOp.OR) {
            return emitShortCircuitOr(binary);
        }

        IRValue left = emitExpr(binary.left());
        IRValue right = emitExpr(binary.right());
        Temp result = newTemp(Type.INT);
        switch (binary.op()) {
            case ADD -> emit(new BinaryOp(result, BinaryOp.Op.ADD, left, right));
            case SUB -> emit(new BinaryOp(result, BinaryOp.Op.SUB, left, right));
            case MUL -> emit(new BinaryOp(result, BinaryOp.Op.MUL, left, right));
            case DIV -> emit(new BinaryOp(result, BinaryOp.Op.DIV, left, right));
            case MOD -> emit(new BinaryOp(result, BinaryOp.Op.MOD, left, right));
            case LT -> emit(new Compare(result, Compare.Predicate.LT, left, right));
            case GT -> emit(new Compare(result, Compare.Predicate.GT, left, right));
            case LE -> emit(new Compare(result, Compare.Predicate.LE, left, right));
            case GE -> emit(new Compare(result, Compare.Predicate.GE, left, right));
            case EQ -> emit(new Compare(result, Compare.Predicate.EQ, left, right));
            case NEQ -> emit(new Compare(result, Compare.Predicate.NE, left, right));
            case AND, OR -> throw new IllegalStateException("short-circuit operator should be handled earlier");
        }
        return result;
    }

    private IRValue emitShortCircuitAnd(Expr.Binary binary) {
        Temp result = newTemp(Type.INT);
        emit(new LoadImm(result, 0));
        BasicBlock rightBlock = appendBlock("land.rhs");
        BasicBlock endBlock = appendBlock("land.end");
        emit(new CondBranch(emitExpr(binary.left()), rightBlock.label(), endBlock.label()));

        setCurrentBlock(rightBlock);
        emit(new Compare(result, Compare.Predicate.NE, emitExpr(binary.right()), Constant.of(0)));
        emitBranchIfOpen(endBlock.label());

        setCurrentBlock(endBlock);
        return result;
    }

    private IRValue emitShortCircuitOr(Expr.Binary binary) {
        Temp result = newTemp(Type.INT);
        emit(new LoadImm(result, 1));
        BasicBlock rightBlock = appendBlock("lor.rhs");
        BasicBlock endBlock = appendBlock("lor.end");
        emit(new CondBranch(emitExpr(binary.left()), endBlock.label(), rightBlock.label()));

        setCurrentBlock(rightBlock);
        emit(new Compare(result, Compare.Predicate.NE, emitExpr(binary.right()), Constant.of(0)));
        emitBranchIfOpen(endBlock.label());

        setCurrentBlock(endBlock);
        return result;
    }

    private IRValue emitCall(Expr.FuncCall call) {
        FunctionSymbol function = semanticResult.functionOf(call);
        Type returnType = toCommonType(function.returnType());
        List<IRValue> args = new ArrayList<>();
        for (Expr arg : call.args()) {
            args.add(emitExpr(arg));
        }
        Temp result = returnType == Type.VOID ? null : newTemp(returnType);
        emit(new Call(result, function.name(), returnType, args));
        return result;
    }

    private void emitCondExpr(Expr expr, Label trueTarget, Label falseTarget) {
        if (expr instanceof Expr.IntLiteral literal) {
            emit(new Branch(literal.value() != 0 ? trueTarget : falseTarget));
            return;
        }
        if (expr instanceof Expr.Unary unary && unary.op() == Expr.UnaryOp.NOT) {
            emitCondExpr(unary.expr(), falseTarget, trueTarget);
            return;
        }
        if (expr instanceof Expr.Binary binary) {
            if (binary.op() == Expr.BinaryOp.AND) {
                BasicBlock rhsBlock = appendBlock("land.rhs");
                emitCondExpr(binary.left(), rhsBlock.label(), falseTarget);
                setCurrentBlock(rhsBlock);
                emitCondExpr(binary.right(), trueTarget, falseTarget);
                return;
            }
            if (binary.op() == Expr.BinaryOp.OR) {
                BasicBlock rhsBlock = appendBlock("lor.rhs");
                emitCondExpr(binary.left(), trueTarget, rhsBlock.label());
                setCurrentBlock(rhsBlock);
                emitCondExpr(binary.right(), trueTarget, falseTarget);
                return;
            }
        }

        emit(new CondBranch(emitExpr(expr), trueTarget, falseTarget));
    }

    private Temp emitLoadImm(int value) {
        Temp result = newTemp(Type.INT);
        emit(new LoadImm(result, value));
        return result;
    }

    private IRValue addressOf(Symbol symbol) {
        return addressOf(symbolValues.get(symbol));
    }

    private IRValue addressOf(IRValue value) {
        if (value instanceof GlobalVar global) {
            Temp address = newTemp(Type.INT);
            emit(new GlobalAddr(address, global));
            return address;
        }
        return value;
    }

    private int constValueOrZero(Expr expr) {
        OptionalInt value = semanticResult.constValueOf(expr);
        return value.orElse(0);
    }

    private Type toCommonType(FuncDef.Type type) {
        return switch (type) {
            case INT -> Type.INT;
            case VOID -> Type.VOID;
        };
    }

    private record LoopTarget(Label continueTarget, Label breakTarget) {
    }
}
