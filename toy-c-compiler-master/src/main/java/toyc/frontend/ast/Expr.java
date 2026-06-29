package toyc.frontend.ast;

import java.util.List;

public sealed interface Expr extends ASTNode permits
        Expr.Binary, Expr.Unary, Expr.Id, Expr.IntLiteral, Expr.FuncCall {

    enum BinaryOp {
        ADD, SUB, MUL, DIV, MOD,
        LT, GT, LE, GE, EQ, NEQ,
        AND, OR
    }

    enum UnaryOp {
        PLUS, MINUS, NOT
    }

    record Binary(Expr left, BinaryOp op, Expr right) implements Expr {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitBinaryExpr(this, context);
        }
    }

    record Unary(UnaryOp op, Expr expr) implements Expr {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitUnaryExpr(this, context);
        }
    }

    record Id(String name) implements Expr {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitIdExpr(this, context);
        }
    }

    record IntLiteral(int value) implements Expr {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitIntLiteralExpr(this, context);
        }
    }

    record FuncCall(String id, List<Expr> args) implements Expr {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitFuncCallExpr(this, context);
        }
    }
}
