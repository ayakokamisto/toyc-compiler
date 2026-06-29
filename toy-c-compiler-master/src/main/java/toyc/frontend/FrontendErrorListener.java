package toyc.frontend;

import org.antlr.v4.runtime.BaseErrorListener;
import org.antlr.v4.runtime.RecognitionException;
import org.antlr.v4.runtime.Recognizer;

public final class FrontendErrorListener extends BaseErrorListener {
    public static final FrontendErrorListener INSTANCE = new FrontendErrorListener();

    private FrontendErrorListener() {
    }

    public static void checkNoUnclosedBlockComment(String source) {
        int line = 1;
        int column = 0;
        for (int i = 0; i < source.length(); i++) {
            char c = source.charAt(i);
            char next = i + 1 < source.length() ? source.charAt(i + 1) : '\0';
            if (c == '/' && next == '/') {
                i++;
                column += 2;
                while (i + 1 < source.length()) {
                    char commentChar = source.charAt(i + 1);
                    if (commentChar == '\r' || commentChar == '\n') {
                        break;
                    }
                    i++;
                    column++;
                }
                continue;
            }
            if (c == '/' && next == '*') {
                int commentLine = line;
                int commentColumn = column;
                i += 2;
                column += 2;
                boolean closed = false;
                while (i < source.length()) {
                    char commentChar = source.charAt(i);
                    char afterCommentChar = i + 1 < source.length() ? source.charAt(i + 1) : '\0';
                    if (commentChar == '*' && afterCommentChar == '/') {
                        i++;
                        column += 2;
                        closed = true;
                        break;
                    }
                    if (commentChar == '\r') {
                        if (afterCommentChar == '\n') {
                            i++;
                        }
                        line++;
                        column = 0;
                    } else if (commentChar == '\n') {
                        line++;
                        column = 0;
                    } else {
                        column++;
                    }
                    i++;
                }
                if (!closed) {
                    throw new IllegalArgumentException(
                            "syntax error at " + commentLine + ":" + commentColumn + ": unterminated block comment");
                }
                continue;
            }
            if (c == '\r') {
                if (next == '\n') {
                    i++;
                }
                line++;
                column = 0;
            } else if (c == '\n') {
                line++;
                column = 0;
            } else {
                column++;
            }
        }
    }

    @Override
    public void syntaxError(
            Recognizer<?, ?> recognizer,
            Object offendingSymbol,
            int line,
            int charPositionInLine,
            String msg,
            RecognitionException e) {
        throw new IllegalArgumentException("syntax error at " + line + ":" + charPositionInLine + ": " + msg, e);
    }
}
