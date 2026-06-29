package toyc.frontend.ast;

public interface ASTNode {
    <R, C> R accept(ASTVisitor<R, C> visitor, C context);
}
