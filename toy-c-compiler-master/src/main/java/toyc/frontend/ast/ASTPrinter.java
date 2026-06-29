package toyc.frontend.ast;

import java.util.StringJoiner;

/**
 * AST 树形打印器，用于调试和验证 AST 结构是否正确。
 * 用法: {@code ASTPrinter.print(program);}
 */
public final class ASTPrinter implements ASTVisitor<Void, Integer> {

    private final StringBuilder sb;
    private static final String INDENT = "  ";

    private ASTPrinter() {
        this.sb = new StringBuilder();
    }

    /** 返回 AST 的格式化字符串 */
    public static String print(Program program) {
        ASTPrinter p = new ASTPrinter();
        program.accept(p, 0);
        return p.sb.toString();
    }

    private void emit(int depth, String text) {
        sb.append(INDENT.repeat(depth)).append(text).append('\n');
    }

    // ========== 顶层 ==========

    @Override
    public Void visitProgram(Program node, Integer depth) {
        emit(depth, "Program");
        for (ASTNode def : node.defs()) {
            def.accept(this, depth + 1);
        }
        return null;
    }

    @Override
    public Void visitFuncDef(FuncDef node, Integer depth) {
        emit(depth, "FuncDef [" + node.returnType() + "] " + node.id());
        if (!node.params().isEmpty()) {
            emit(depth + 1, "Params:");
            for (FuncDef.Param p : node.params()) {
                p.accept(this, depth + 2);
            }
        }
        node.body().accept(this, depth + 1);
        return null;
    }

    @Override
    public Void visitFuncDefParam(FuncDef.Param node, Integer depth) {
        emit(depth, "Param [" + node.id() + "]");
        return null;
    }

    // ========== 声明 ==========

    @Override
    public Void visitVarDecl(Decl.VarDecl node, Integer depth) {
        emit(depth, "VarDecl [" + node.id() + "]");
        node.init().accept(this, depth + 1);
        return null;
    }

    @Override
    public Void visitConstDecl(Decl.ConstDecl node, Integer depth) {
        emit(depth, "ConstDecl [" + node.id() + "]");
        node.init().accept(this, depth + 1);
        return null;
    }

    // ========== 语句 ==========

    @Override
    public Void visitBlockStmt(Stmt.Block node, Integer depth) {
        emit(depth, "Block");
        for (Stmt s : node.stmts()) {
            s.accept(this, depth + 1);
        }
        return null;
    }

    @Override
    public Void visitEmptyStmt(Stmt.Empty node, Integer depth) {
        emit(depth, "Empty");
        return null;
    }

    @Override
    public Void visitExprStmt(Stmt.ExprStmt node, Integer depth) {
        emit(depth, "ExprStmt");
        node.expr().accept(this, depth + 1);
        return null;
    }

    @Override
    public Void visitAssignStmt(Stmt.Assign node, Integer depth) {
        emit(depth, "Assign [" + node.id() + "]");
        node.expr().accept(this, depth + 1);
        return null;
    }

    @Override
    public Void visitIfStmt(Stmt.If node, Integer depth) {
        emit(depth, "If");
        emit(depth + 1, "Cond:");
        node.cond().accept(this, depth + 2);
        emit(depth + 1, "Then:");
        node.thenStmt().accept(this, depth + 2);
        if (node.elseStmt() != null) {
            emit(depth + 1, "Else:");
            node.elseStmt().accept(this, depth + 2);
        }
        return null;
    }

    @Override
    public Void visitWhileStmt(Stmt.While node, Integer depth) {
        emit(depth, "While");
        emit(depth + 1, "Cond:");
        node.cond().accept(this, depth + 2);
        emit(depth + 1, "Body:");
        node.body().accept(this, depth + 2);
        return null;
    }

    @Override
    public Void visitBreakStmt(Stmt.Break node, Integer depth) {
        emit(depth, "Break");
        return null;
    }

    @Override
    public Void visitContinueStmt(Stmt.Continue node, Integer depth) {
        emit(depth, "Continue");
        return null;
    }

    @Override
    public Void visitReturnStmt(Stmt.Return node, Integer depth) {
        if (node.expr() != null) {
            emit(depth, "Return:");
            node.expr().accept(this, depth + 1);
        } else {
            emit(depth, "Return");
        }
        return null;
    }

    // ========== 表达式 ==========

    @Override
    public Void visitBinaryExpr(Expr.Binary node, Integer depth) {
        emit(depth, "Binary [" + node.op() + "]");
        node.left().accept(this, depth + 1);
        node.right().accept(this, depth + 1);
        return null;
    }

    @Override
    public Void visitUnaryExpr(Expr.Unary node, Integer depth) {
        emit(depth, "Unary [" + node.op() + "]");
        node.expr().accept(this, depth + 1);
        return null;
    }

    @Override
    public Void visitIdExpr(Expr.Id node, Integer depth) {
        emit(depth, "Id [" + node.name() + "]");
        return null;
    }

    @Override
    public Void visitIntLiteralExpr(Expr.IntLiteral node, Integer depth) {
        emit(depth, "IntLiteral [" + node.value() + "]");
        return null;
    }

    @Override
    public Void visitFuncCallExpr(Expr.FuncCall node, Integer depth) {
        emit(depth, "FuncCall [" + node.id() + "]");
        if (!node.args().isEmpty()) {
            for (Expr arg : node.args()) {
                arg.accept(this, depth + 1);
            }
        }
        return null;
    }
}
