package toyc.frontend;

import org.antlr.v4.runtime.tree.ParseTree;
import org.antlr.v4.runtime.tree.TerminalNode;
import toyc.frontend.ast.*;
import toyc.frontend.parser.ToyCBaseVisitor;
import toyc.frontend.parser.ToyCParser;

import java.util.ArrayList;
import java.util.List;

public class ASTBuilder extends ToyCBaseVisitor<ASTNode> {

    @Override
    public ASTNode visitCompUnit(ToyCParser.CompUnitContext ctx) {
        List<ASTNode> defs = new ArrayList<>();
        for (ParseTree child : ctx.children) {
            if (child instanceof ToyCParser.DeclContext || child instanceof ToyCParser.FuncDefContext) {
                defs.add(visit(child));
            }
        }
        return new Program(defs);
    }

    @Override
    public ASTNode visitDecl(ToyCParser.DeclContext ctx) {
        if (ctx.varDecl() != null) return visit(ctx.varDecl());
        if (ctx.constDecl() != null) return visit(ctx.constDecl());
        throw new IllegalStateException("Unknown decl");
    }

    @Override
    public ASTNode visitConstDecl(ToyCParser.ConstDeclContext ctx) {
        return new Decl.ConstDecl(ctx.ID().getText(), (Expr) visit(ctx.expr()));
    }

    @Override
    public ASTNode visitVarDecl(ToyCParser.VarDeclContext ctx) {
        return new Decl.VarDecl(ctx.ID().getText(), (Expr) visit(ctx.expr()));
    }

    @Override
    public ASTNode visitFuncDef(ToyCParser.FuncDefContext ctx) {
        FuncDef.Type type = ctx.INT() != null ? FuncDef.Type.INT : FuncDef.Type.VOID;
        String id = ctx.ID().getText();
        List<FuncDef.Param> params = new ArrayList<>();
        for (ToyCParser.ParamContext pCtx : ctx.param()) {
            params.add((FuncDef.Param) visit(pCtx));
        }
        Stmt.Block block = (Stmt.Block) visit(ctx.block());
        return new FuncDef(type, id, params, block);
    }

    @Override
    public ASTNode visitParam(ToyCParser.ParamContext ctx) {
        return new FuncDef.Param(ctx.ID().getText());
    }

    @Override
    public ASTNode visitBlock(ToyCParser.BlockContext ctx) {
        List<Stmt> stmts = new ArrayList<>();
        for (ToyCParser.StmtContext sCtx : ctx.stmt()) {
            stmts.add((Stmt) visit(sCtx));
        }
        return new Stmt.Block(stmts);
    }

    @Override
    public ASTNode visitStmt(ToyCParser.StmtContext ctx) {
        if (ctx.block() != null) return visit(ctx.block());
        if (ctx.decl() != null) return visit(ctx.decl());
        if (ctx.IF() != null) {
            Expr cond = (Expr) visit(ctx.expr());
            Stmt thenStmt = (Stmt) visit(ctx.stmt(0));
            Stmt elseStmt = ctx.ELSE() != null ? (Stmt) visit(ctx.stmt(1)) : null;
            return new Stmt.If(cond, thenStmt, elseStmt);
        }
        if (ctx.WHILE() != null) {
            return new Stmt.While((Expr) visit(ctx.expr()), (Stmt) visit(ctx.stmt(0)));
        }
        if (ctx.BREAK() != null) return new Stmt.Break();
        if (ctx.CONTINUE() != null) return new Stmt.Continue();
        if (ctx.RETURN() != null) {
            Expr expr = ctx.expr() != null ? (Expr) visit(ctx.expr()) : null;
            return new Stmt.Return(expr);
        }
        if (ctx.ASSIGN() != null) {
            return new Stmt.Assign(ctx.ID().getText(), (Expr) visit(ctx.expr()));
        }
        // expr SEMI  (checked after keyword-based alternatives above)
        if (ctx.expr() != null) {
            return new Stmt.ExprStmt((Expr) visit(ctx.expr()));
        }
        // Bare SEMI (';')
        return new Stmt.Empty();
    }

    @Override
    public ASTNode visitExpr(ToyCParser.ExprContext ctx) {
        return visit(ctx.lOrExpr());
    }

    @Override
    public ASTNode visitLOrExpr(ToyCParser.LOrExprContext ctx) {
        Expr left = (Expr) visit(ctx.lAndExpr(0));
        for (int i = 1; i < ctx.lAndExpr().size(); i++) {
            Expr right = (Expr) visit(ctx.lAndExpr(i));
            left = new Expr.Binary(left, Expr.BinaryOp.OR, right);
        }
        return left;
    }

    @Override
    public ASTNode visitLAndExpr(ToyCParser.LAndExprContext ctx) {
        Expr left = (Expr) visit(ctx.relExpr(0));
        for (int i = 1; i < ctx.relExpr().size(); i++) {
            Expr right = (Expr) visit(ctx.relExpr(i));
            left = new Expr.Binary(left, Expr.BinaryOp.AND, right);
        }
        return left;
    }

    @Override
    public ASTNode visitRelExpr(ToyCParser.RelExprContext ctx) {
        Expr left = (Expr) visit(ctx.addExpr(0));
        for (int i = 1; i < ctx.addExpr().size(); i++) {
            int opType = ((TerminalNode) ctx.getChild(2 * i - 1)).getSymbol().getType();
            Expr right = (Expr) visit(ctx.addExpr(i));
            left = new Expr.Binary(left, toBinaryOp(opType), right);
        }
        return left;
    }

    @Override
    public ASTNode visitAddExpr(ToyCParser.AddExprContext ctx) {
        Expr left = (Expr) visit(ctx.mulExpr(0));
        for (int i = 1; i < ctx.mulExpr().size(); i++) {
            int opType = ((TerminalNode) ctx.getChild(2 * i - 1)).getSymbol().getType();
            Expr right = (Expr) visit(ctx.mulExpr(i));
            left = new Expr.Binary(left, toBinaryOp(opType), right);
        }
        return left;
    }

    @Override
    public ASTNode visitMulExpr(ToyCParser.MulExprContext ctx) {
        Expr left = (Expr) visit(ctx.unaryExpr(0));
        for (int i = 1; i < ctx.unaryExpr().size(); i++) {
            int opType = ((TerminalNode) ctx.getChild(2 * i - 1)).getSymbol().getType();
            Expr right = (Expr) visit(ctx.unaryExpr(i));
            left = new Expr.Binary(left, toBinaryOp(opType), right);
        }
        return left;
    }

    @Override
    public ASTNode visitUnaryExpr(ToyCParser.UnaryExprContext ctx) {
        if (ctx.primaryExpr() != null) {
            return visit(ctx.primaryExpr());
        }
        int opType = ((TerminalNode) ctx.getChild(0)).getSymbol().getType();
        Expr.UnaryOp op = switch (opType) {
            case ToyCParser.PLUS -> Expr.UnaryOp.PLUS;
            case ToyCParser.MINUS -> Expr.UnaryOp.MINUS;
            case ToyCParser.NOT -> Expr.UnaryOp.NOT;
            default -> throw new IllegalArgumentException("Unknown unary operator: " + opType);
        };
        return new Expr.Unary(op, (Expr) visit(ctx.unaryExpr()));
    }

    @Override
    public ASTNode visitPrimaryExpr(ToyCParser.PrimaryExprContext ctx) {
        if (ctx.NUMBER() != null) {
            return new Expr.IntLiteral(Integer.parseInt(ctx.NUMBER().getText()));
        }
        if (ctx.LPAREN() != null && ctx.expr() != null && ctx.ID() == null) {
            // ( expr )
            return visit(ctx.expr(0));
        }
        if (ctx.ID() != null) {
            if (ctx.LPAREN() != null) {
                // ID ( args )
                List<Expr> args = new ArrayList<>();
                for (ToyCParser.ExprContext eCtx : ctx.expr()) {
                    args.add((Expr) visit(eCtx));
                }
                return new Expr.FuncCall(ctx.ID().getText(), args);
            } else {
                // ID
                return new Expr.Id(ctx.ID().getText());
            }
        }
        throw new IllegalStateException("Unknown primary expression");
    }

    private Expr.BinaryOp toBinaryOp(int tokenType) {
        return switch (tokenType) {
            case ToyCParser.PLUS -> Expr.BinaryOp.ADD;
            case ToyCParser.MINUS -> Expr.BinaryOp.SUB;
            case ToyCParser.STAR -> Expr.BinaryOp.MUL;
            case ToyCParser.SLASH -> Expr.BinaryOp.DIV;
            case ToyCParser.PERCENT -> Expr.BinaryOp.MOD;
            case ToyCParser.LT -> Expr.BinaryOp.LT;
            case ToyCParser.GT -> Expr.BinaryOp.GT;
            case ToyCParser.LE -> Expr.BinaryOp.LE;
            case ToyCParser.GE -> Expr.BinaryOp.GE;
            case ToyCParser.EQ -> Expr.BinaryOp.EQ;
            case ToyCParser.NE -> Expr.BinaryOp.NEQ;
            case ToyCParser.AND -> Expr.BinaryOp.AND;
            case ToyCParser.OR -> Expr.BinaryOp.OR;
            default -> throw new IllegalArgumentException("Unknown binary operator type: " + tokenType);
        };
    }
}
