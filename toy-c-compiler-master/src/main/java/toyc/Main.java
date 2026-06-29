package toyc;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import toyc.backend.RiscVEmitter;
import toyc.frontend.LexerFacade;
import toyc.frontend.ParserFacade;
import toyc.frontend.ast.ASTPrinter;
import toyc.frontend.ast.Program;
import toyc.ir.IRBuilder;
import toyc.ir.IRProgram;
import toyc.opt.Optimizer;
import toyc.semantic.SemanticAnalyzer;
import toyc.semantic.SemanticResult;

public final class Main {
    private Main() {
    }

    public static void main(String[] args) {
        try {
            boolean opt = hasFlag(args, "-opt");
            boolean dumpAst = hasFlag(args, "--dump-ast");
            boolean dumpTokens = hasFlag(args, "--dump-tokens");
            String source = new String(System.in.readAllBytes(), StandardCharsets.UTF_8);

            if (dumpTokens) {
                for (LexerFacade.TokenInfo token : LexerFacade.scan(source)) {
                    System.out.printf("%d:%d %s %s%n", token.line(), token.column(), token.type(), token.text());
                }
                return;
            }

            Program ast = ParserFacade.parse(source);

            if (dumpAst) {
                System.out.println(ASTPrinter.print(ast));
                return;
            }

            SemanticResult sem = SemanticAnalyzer.analyze(ast);
            IRProgram ir = IRBuilder.build(ast, sem);
            if (opt) {
                ir = Optimizer.optimize(ir);
            }
            String asm = RiscVEmitter.emit(ir, sem);
            System.out.print(asm);
        } catch (IOException | RuntimeException ex) {
            System.err.println("toyc: compilation failed: " + ex.getMessage());
            System.exit(1);
        }
    }

    private static boolean hasFlag(String[] args, String flag) {
        for (String arg : args) {
            if (flag.equals(arg)) {
                return true;
            }
        }
        return false;
    }
}
