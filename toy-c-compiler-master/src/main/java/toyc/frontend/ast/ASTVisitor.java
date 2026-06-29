package toyc.frontend.ast;

public interface ASTVisitor<R, C> {
    R visitProgram(Program node, C context);
    
    R visitFuncDef(FuncDef node, C context);
    R visitFuncDefParam(FuncDef.Param node, C context);
    
    R visitVarDecl(Decl.VarDecl node, C context);
    R visitConstDecl(Decl.ConstDecl node, C context);
    
    R visitBlockStmt(Stmt.Block node, C context);
    R visitEmptyStmt(Stmt.Empty node, C context);
    R visitExprStmt(Stmt.ExprStmt node, C context);
    R visitAssignStmt(Stmt.Assign node, C context);
    R visitIfStmt(Stmt.If node, C context);
    R visitWhileStmt(Stmt.While node, C context);
    R visitBreakStmt(Stmt.Break node, C context);
    R visitContinueStmt(Stmt.Continue node, C context);
    R visitReturnStmt(Stmt.Return node, C context);
    
    R visitBinaryExpr(Expr.Binary node, C context);
    R visitUnaryExpr(Expr.Unary node, C context);
    R visitIdExpr(Expr.Id node, C context);
    R visitIntLiteralExpr(Expr.IntLiteral node, C context);
    R visitFuncCallExpr(Expr.FuncCall node, C context);
}
