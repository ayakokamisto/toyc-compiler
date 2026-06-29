package toyc.frontend;

import java.util.ArrayList;
import java.util.List;
import org.antlr.v4.runtime.CharStreams;
import org.antlr.v4.runtime.Token;
import org.antlr.v4.runtime.Vocabulary;
import toyc.frontend.parser.ToyCLexer;

public final class LexerFacade {
    private LexerFacade() {
    }

    public static List<TokenInfo> scan(String source) {
        FrontendErrorListener.checkNoUnclosedBlockComment(source);
        ToyCLexer lexer = new ToyCLexer(CharStreams.fromString(source));
        lexer.removeErrorListeners();
        lexer.addErrorListener(FrontendErrorListener.INSTANCE);

        Vocabulary vocabulary = lexer.getVocabulary();
        List<TokenInfo> tokens = new ArrayList<>();
        for (Token token = lexer.nextToken(); token.getType() != Token.EOF; token = lexer.nextToken()) {
            tokens.add(new TokenInfo(
                    tokenName(vocabulary, token.getType()),
                    token.getText(),
                    token.getLine(),
                    token.getCharPositionInLine()));
        }
        return List.copyOf(tokens);
    }

    private static String tokenName(Vocabulary vocabulary, int tokenType) {
        String symbolicName = vocabulary.getSymbolicName(tokenType);
        if (symbolicName != null) {
            return symbolicName;
        }
        String literalName = vocabulary.getLiteralName(tokenType);
        if (literalName != null) {
            return literalName;
        }
        return Integer.toString(tokenType);
    }

    public record TokenInfo(String type, String text, int line, int column) {
    }
}
