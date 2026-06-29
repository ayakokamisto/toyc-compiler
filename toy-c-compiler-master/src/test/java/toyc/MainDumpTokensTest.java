package toyc;

import static org.junit.jupiter.api.Assertions.assertEquals;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import org.junit.jupiter.api.Test;

class MainDumpTokensTest {
    @Test
    void dumpsTokenStreamWithoutParsingProgram() {
        ByteArrayInputStream input = new ByteArrayInputStream("int main() { return 0; }".getBytes(StandardCharsets.UTF_8));
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        PrintStream originalOut = System.out;
        java.io.InputStream originalIn = System.in;
        try {
            System.setIn(input);
            System.setOut(new PrintStream(output, true, StandardCharsets.UTF_8));

            Main.main(new String[] {"--dump-tokens"});
        } finally {
            System.setIn(originalIn);
            System.setOut(originalOut);
        }

        String actual = output.toString(StandardCharsets.UTF_8).replace("\r\n", "\n");

        assertEquals("""
                1:0 INT int
                1:4 ID main
                1:8 LPAREN (
                1:9 RPAREN )
                1:11 LBRACE {
                1:13 RETURN return
                1:20 NUMBER 0
                1:21 SEMI ;
                1:23 RBRACE }
                """, actual);
    }
}
