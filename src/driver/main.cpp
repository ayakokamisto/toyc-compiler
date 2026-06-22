#include "lexer/lexer.h"

#include <iostream>
#include <iterator>
#include <string>
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

        (void)options;
        std::cerr << "compilation stopped: parser, semantic analysis, and code generation are incomplete\n";
        return 1;
    } catch (const toyc::LexError& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
