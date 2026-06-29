package toyc.frontend;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.List;
import org.junit.jupiter.api.Test;

class LexerFacadeTest {
    @Test
    void distinguishesKeywordsFromIdentifiers() {
        List<String> types = tokenTypes("int integer while while1 const const_value");

        assertEquals(List.of("INT", "ID", "WHILE", "ID", "CONST", "ID"), types);
    }

    @Test
    void recognizesCompoundAndSingleCharacterOperators() {
        List<String> types = tokenTypes("<= >= == != && || = + - * / % !");

        assertEquals(List.of(
                "LE", "GE", "EQ", "NE", "AND", "OR",
                "ASSIGN", "PLUS", "MINUS", "STAR", "SLASH", "PERCENT", "NOT"), types);
    }

    @Test
    void keepsMinusSeparateFromNumbers() {
        List<String> types = tokenTypes("1-5 -42");

        assertEquals(List.of("NUMBER", "MINUS", "NUMBER", "MINUS", "NUMBER"), types);
    }

    @Test
    void skipsWhitespaceAndCommentsWhileKeepingTokenLocations() {
        List<LexerFacade.TokenInfo> tokens = LexerFacade.scan("int main // line comment\n/* block\n   comment */ return\n");

        assertEquals(List.of("INT", "ID", "RETURN"), tokenTypes(tokens));
        assertEquals(new LexerFacade.TokenInfo("INT", "int", 1, 0), tokens.get(0));
        assertEquals(new LexerFacade.TokenInfo("ID", "main", 1, 4), tokens.get(1));
        assertEquals(new LexerFacade.TokenInfo("RETURN", "return", 3, 14), tokens.get(2));
    }

    @Test
    void rejectsInvalidCharacters() {
        IllegalArgumentException ex = assertThrows(
                IllegalArgumentException.class,
                () -> LexerFacade.scan("int @"));

        assertTrue(ex.getMessage().startsWith("syntax error at 1:4:"));
    }

    @Test
    void rejectsUnclosedBlockComment() {
        IllegalArgumentException ex = assertThrows(
                IllegalArgumentException.class,
                () -> LexerFacade.scan("int /* unclosed"));

        assertTrue(ex.getMessage().startsWith("syntax error at 1:4:"));
    }

    private static List<String> tokenTypes(String source) {
        return tokenTypes(LexerFacade.scan(source));
    }

    private static List<String> tokenTypes(List<LexerFacade.TokenInfo> tokens) {
        return tokens.stream()
                .map(LexerFacade.TokenInfo::type)
                .toList();
    }
}
