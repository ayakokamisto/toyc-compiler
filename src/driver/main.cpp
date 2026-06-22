#include "common/token_stream.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct DriverOptions {
    bool optimize = false;
    bool dumpTokens = false;
};

bool parseArgs(int argc, char** argv, DriverOptions& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-opt") {
            options.optimize = true;
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

} // namespace

int main(int argc, char** argv) {
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
        for (const toyc::Diagnostic& diagnostic : parser.diagnostics()) {
            printDiagnostic(diagnostic);
        }
        if (parser.hasError()) {
            return 1;
        }

        (void)unit;
        (void)options;
        return 0;
    } catch (const toyc::LexError& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
