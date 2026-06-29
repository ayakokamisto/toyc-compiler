package toyc.frontend.ast;

import java.util.List;

public sealed interface Stmt extends ASTNode permits
        Stmt.Block, Stmt.Empty, Stmt.ExprStmt, Stmt.Assign, Decl,
        Stmt.If, Stmt.While, Stmt.Break, Stmt.Continue, Stmt.Return {

    record Block(List<Stmt> stmts) implements Stmt {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitBlockStmt(this, context);
        }
    }

    record Empty() implements Stmt {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitEmptyStmt(this, context);
        }
    }

    record ExprStmt(Expr expr) implements Stmt {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitExprStmt(this, context);
        }
    }

    record Assign(String id, Expr expr) implements Stmt {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitAssignStmt(this, context);
        }
    }

    record If(Expr cond, Stmt thenStmt, Stmt elseStmt) implements Stmt {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitIfStmt(this, context);
        }
    }

    record While(Expr cond, Stmt body) implements Stmt {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitWhileStmt(this, context);
        }
    }

    record Break() implements Stmt {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitBreakStmt(this, context);
        }
    }

    record Continue() implements Stmt {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitContinueStmt(this, context);
        }
    }

    record Return(Expr expr) implements Stmt {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitReturnStmt(this, context);
        }
    }
}
