#include "codegen/BackendOptions.h"
#include "codegen/RiscvBackend.h"
#include "common/token_stream.h"
#include "ir/contract_ir_generator.h"
#include "ir/ir_optimizer.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/sema.h"

#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

struct DriverOptions {
    bool optimize = false;
    toyc::OptimizationLevel optLevel = toyc::OptimizationLevel::O3OJ;
    bool dumpTokens = false;
};

bool parseArgs(int argc, char** argv, DriverOptions& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-opt") {
            options.optimize = true;
            options.optLevel = toyc::OptimizationLevel::O3OJ;
        } else if (arg == "-opt=o1" || arg == "-opt=O1") {
            options.optimize = true;
            options.optLevel = toyc::OptimizationLevel::O1Safe;
        } else if (arg == "-opt=o2" || arg == "-opt=O2") {
            options.optimize = true;
            options.optLevel = toyc::OptimizationLevel::O2Backend;
        } else if (arg == "-opt=o3" || arg == "-opt=O3") {
            options.optimize = true;
            options.optLevel = toyc::OptimizationLevel::O3OJ;
        } else if (arg == "--dump-tokens") {
            options.dumpTokens = true;
        } else {
            std::cerr << "unknown option: " << arg << '\n';
            return false;
        }
    }
    return true;
}

void dumpTokens(const std::vector<toyc::Token>& tokens) {
    for (const toyc::Token& token : tokens) {
        std::cerr << token.location.line << ':' << token.location.column << ' '
                  << toyc::tokenKindToString(token.kind) << " '" << token.lexeme << "'\n";
    }
}

bool hasError(const std::vector<toyc::Diagnostic>& diagnostics) {
    for (const toyc::Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity == toyc::DiagnosticSeverity::Error) {
            return true;
        }
    }
    return false;
}

void printDiagnostic(const toyc::Diagnostic& diagnostic) {
    const char* severity = diagnostic.severity == toyc::DiagnosticSeverity::Error
                               ? "error"
                               : "warning";
    std::string_view message = diagnostic.message;
    const std::size_t firstColon = message.find(':');
    const std::size_t secondColon =
        firstColon == std::string_view::npos ? std::string_view::npos
                                             : message.find(':', firstColon + 1);
    if (secondColon != std::string_view::npos && secondColon + 1 < message.size() &&
        message[secondColon + 1] == ' ') {
        message.remove_prefix(secondColon + 2);
    }
    std::cerr << diagnostic.range.begin.line << ':' << diagnostic.range.begin.column << ": "
              << severity << ": " << message << '\n';
}

void printDiagnostics(const std::vector<toyc::Diagnostic>& diagnostics) {
    for (const toyc::Diagnostic& diagnostic : diagnostics) {
        printDiagnostic(diagnostic);
    }
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    DriverOptions options;
    if (!parseArgs(argc, argv, options)) {
        return 2;
    }

    const std::string source(std::istreambuf_iterator<char>(std::cin),
                             std::istreambuf_iterator<char>());

    try {
        const std::vector<toyc::Token> tokens = toyc::lex(source);
        if (options.dumpTokens) {
            dumpTokens(tokens);
            return 0;
        }

        toyc::TokenStream tokenStream(tokens);
        toyc::parser::Parser parser(tokenStream);
        const std::unique_ptr<toyc::ast::CompUnit> unit = parser.parseCompUnit();
        printDiagnostics(parser.diagnostics());
        if (parser.hasError() || hasError(parser.diagnostics())) {
            return 1;
        }

        toyc::sema::Sema sema;
        toyc::sema::SemaResult semaResult = sema.analyze(*unit);
        printDiagnostics(semaResult.diagnostics);
        if (hasError(semaResult.diagnostics)) {
            return 1;
        }

        toyc::ir::ContractIRGenerator irGenerator;
        toyc::codegen::contract::IRModule module =
            irGenerator.generate(*unit, semaResult.model);
        printDiagnostics(irGenerator.diagnostics());
        if (hasError(irGenerator.diagnostics())) {
            return 1;
        }

        std::vector<toyc::Diagnostic> verifierDiagnostics;
        const bool verificationSucceeded =
            toyc::ir::verifyContractModule(module, verifierDiagnostics);
        printDiagnostics(verifierDiagnostics);
        if (!verificationSucceeded || hasError(verifierDiagnostics)) {
            return 1;
        }

        toyc::codegen::BackendOptions backendOptions;
        backendOptions.enableOpt = options.optimize;
        backendOptions.optLevel = options.optLevel;

        if (backendOptions.enableOpt) {
            toyc::ir::IROptimizer optimizer;
            toyc::ir::IROptimizerOptions optOptions;
            optOptions.level = options.optLevel;
            optOptions.enableLICM = options.optLevel == toyc::OptimizationLevel::O3OJ;
            (void)optimizer.run(module, optOptions);

            std::vector<toyc::Diagnostic> postOptDiagnostics;
            const bool postOptOk =
                toyc::ir::verifyContractModule(module, postOptDiagnostics);
            printDiagnostics(postOptDiagnostics);
            if (!postOptOk || hasError(postOptDiagnostics)) {
                return 1;
            }
        }

        const std::string assembly =
            toyc::codegen::RiscvBackend().generate(module, backendOptions);
        std::fwrite(assembly.data(), 1, assembly.size(), stdout);
        return 0;
    } catch (const toyc::LexError& error) {
        std::cerr << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "internal error: " << error.what() << '\n';
        return 1;
    }
}
