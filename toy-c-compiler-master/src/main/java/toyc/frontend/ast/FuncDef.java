package toyc.frontend.ast;

import java.util.List;

public record FuncDef(Type returnType, String id, List<Param> params, Stmt.Block body) implements ASTNode {
    
    public enum Type {
        INT, VOID
    }

    public record Param(String id) implements ASTNode {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitFuncDefParam(this, context);
        }
    }

    @Override
    public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
        return visitor.visitFuncDef(this, context);
    }
}
