package toyc.frontend.ast;

import java.util.List;

public record Program(List<ASTNode> defs) implements ASTNode {
    @Override
    public <R, C> R accept(ASTVisitor<R, C> visitor, C context) {
        return visitor.visitProgram(this, context);
    }
}
