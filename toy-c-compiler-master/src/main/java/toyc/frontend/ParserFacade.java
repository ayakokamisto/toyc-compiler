package toyc.frontend;

import org.antlr.v4.runtime.CharStreams;
import org.antlr.v4.runtime.CommonTokenStream;
import toyc.frontend.ast.ASTNode;
import toyc.frontend.ast.Program;
import toyc.frontend.parser.ToyCLexer;
import toyc.frontend.parser.ToyCParser;

public final class ParserFacade {
    private ParserFacade() {
    }

    public static Program parse(String source) {
        FrontendErrorListener.checkNoUnclosedBlockComment(source);
        ToyCLexer lexer = new ToyCLexer(CharStreams.fromString(source));
        lexer.removeErrorListeners();
        lexer.addErrorListener(FrontendErrorListener.INSTANCE);

        CommonTokenStream tokens = new CommonTokenStream(lexer);
        ToyCParser parser = new ToyCParser(tokens);
        parser.removeErrorListeners();
        parser.addErrorListener(FrontendErrorListener.INSTANCE);

        ToyCParser.CompUnitContext ctx = parser.compUnit();
        ASTBuilder builder = new ASTBuilder();
        return (Program) builder.visit(ctx);
    }
}
