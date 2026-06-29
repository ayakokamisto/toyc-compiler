package toyc.frontend.ast;

public sealed interface Decl extends Stmt {
    
    record VarDecl(String id, Expr init) implements Decl {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitVarDecl(this, context);
        }
    }

    record ConstDecl(String id, Expr init) implements Decl {
        @Override
        public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
            return visitor.visitConstDecl(this, context);
        }
    }
}
