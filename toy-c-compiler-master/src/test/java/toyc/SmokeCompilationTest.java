package toyc;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assumptions.assumeTrue;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.stream.Stream;
import org.junit.jupiter.api.Test;
import toyc.backend.RiscVEmitter;
import toyc.frontend.ParserFacade;
import toyc.frontend.ast.Program;
import toyc.ir.IRBuilder;
import toyc.ir.IRProgram;
import toyc.semantic.SemanticAnalyzer;
import toyc.semantic.SemanticResult;

final class SmokeCompilationTest {
    private static final Path SMOKE_DIR = Path.of("src/test/resources/smoke");
    private static final Path ERROR_DIR = Path.of("src/test/resources/errors");
    private static final Path RUNTIME_START = Path.of("src/test/resources/runtime/start_rv32.S");
    private static final Path TEMP_ROOT = Path.of(System.getProperty("java.io.tmpdir"), "toyc-compiler");
    private static final DateTimeFormatter TEMP_DIR_FORMAT = DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss-SSSSSSSSS");
    private static final Duration PROCESS_TIMEOUT = Duration.ofSeconds(10);
    private static final Path TEST_RUN_DIR = createTestRunDir();

    @Test
    void generatedAssemblyMatchesSnapshots() throws Exception {
        for (Path expectedAsm : listMatching("*.expected.s")) {
            Path source = sourceFor(expectedAsm, ".expected.s");
            String actual = compileToAssembly(source);
            String expected = Files.readString(expectedAsm, StandardCharsets.UTF_8);
            assertEquals(expected, actual, "assembly snapshot mismatch for " + source);
        }
    }

    @Test
    void generatedProgramsRunUnderQemu() throws Exception {
        assumeTrue(commandExists("riscv64-unknown-elf-gcc"),
                "riscv64-unknown-elf-gcc is required for RV32 smoke execution tests");
        assumeTrue(commandExists("qemu-riscv32"),
                "qemu-riscv32 is required for RV32 smoke execution tests");

        for (Path expectedExitFile : listMatching("*.expected.exit")) {
            Path source = sourceFor(expectedExitFile, ".expected.exit");
            int expectedExit = Integer.parseInt(Files.readString(expectedExitFile, StandardCharsets.UTF_8).trim());
            int actualExit = compileLinkAndRun(source);
            assertEquals(expectedExit, actualExit, "QEMU exit code mismatch for " + source);
        }
    }

    @Test
    void invalidProgramsReportExpectedErrors() throws Exception {
        for (Path expectedErrorFile : listMatching(ERROR_DIR, "*.expected.error")) {
            Path source = sourceFor(expectedErrorFile, ".expected.error");
            String expectedError = Files.readString(expectedErrorFile, StandardCharsets.UTF_8).trim();
            Exception ex = org.junit.jupiter.api.Assertions.assertThrows(
                    RuntimeException.class,
                    () -> compileToAssembly(source),
                    "expected compilation to fail for " + source);
            assertTrue(ex.getMessage().contains(expectedError),
                    "expected error containing '" + expectedError + "' for " + source + " but got: " + ex.getMessage());
        }
    }

    private static List<Path> listMatching(Path directory, String glob) throws IOException {
        List<Path> paths = new ArrayList<>();
        if (!Files.isDirectory(directory)) {
            return paths;
        }
        try (Stream<Path> stream = Files.list(directory)) {
            stream.filter(path -> path.getFileSystem().getPathMatcher("glob:" + glob)
                    .matches(path.getFileName()))
                    .sorted()
                    .forEach(paths::add);
        }
        return paths;
    }

    private static List<Path> listMatching(String glob) throws IOException {
        return listMatching(SMOKE_DIR, glob);
    }

    private static Path sourceFor(Path expectedFile, String suffix) {
        String fileName = expectedFile.getFileName().toString();
        String sourceName = fileName.substring(0, fileName.length() - suffix.length()) + ".tc";
        return expectedFile.resolveSibling(sourceName);
    }

    private static String compileToAssembly(Path sourcePath) throws IOException {
        String source = Files.readString(sourcePath, StandardCharsets.UTF_8);
        Program ast = ParserFacade.parse(source);
        SemanticResult sem = SemanticAnalyzer.analyze(ast);
        IRProgram ir = IRBuilder.build(ast, sem);
        return RiscVEmitter.emit(ir, sem);
    }

    private static int compileLinkAndRun(Path source) throws Exception {
        String baseName = source.getFileName().toString().replace(".tc", "");
        Path tempDir = TEST_RUN_DIR.resolve(baseName);
        Files.createDirectories(tempDir);
        Path asm = tempDir.resolve(baseName + ".s");
        Path elf = tempDir.resolve(baseName + ".elf");
        Files.writeString(asm, compileToAssembly(source), StandardCharsets.UTF_8);

        ProcessResult gcc = runProcess(List.of(
                "riscv64-unknown-elf-gcc",
                "-march=rv32im",
                "-mabi=ilp32",
                "-nostdlib",
                "-nostartfiles",
                RUNTIME_START.toString(),
                asm.toString(),
                "-o",
                elf.toString()));
        assertEquals(0, gcc.exitCode(), "RV32 link failed:\n" + gcc.output());

        ProcessResult qemu = runProcess(List.of("qemu-riscv32", elf.toString()));
        return qemu.exitCode();
    }

    private static Path createTestRunDir() {
        String timestamp = LocalDateTime.now().format(TEMP_DIR_FORMAT);
        Path runDir = TEMP_ROOT.resolve(timestamp);
        try {
            Files.createDirectories(runDir);
        } catch (IOException ex) {
            throw new IllegalStateException("failed to create smoke test temp directory: " + runDir, ex);
        }
        return runDir;
    }

    private static ProcessResult runProcess(List<String> command) throws Exception {
        Process process = new ProcessBuilder(command)
                .redirectErrorStream(true)
                .start();
        boolean completed = process.waitFor(PROCESS_TIMEOUT.toSeconds(), TimeUnit.SECONDS);
        if (!completed) {
            process.destroyForcibly();
            assertTrue(false, "process timed out: " + String.join(" ", command));
        }
        String output = new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8);
        return new ProcessResult(process.exitValue(), output);
    }

    private static boolean commandExists(String command) {
        try {
            ProcessResult result = runProcess(List.of("sh", "-c", "command -v " + command));
            return result.exitCode() == 0;
        } catch (Exception ex) {
            return false;
        }
    }

    private record ProcessResult(int exitCode, String output) {
    }
}
