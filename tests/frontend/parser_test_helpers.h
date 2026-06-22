#pragma once

#include "ast/ast.h"
#include "common/diagnostic.h"
#include "common/token_stream.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

namespace toyc::test {

[[noreturn]] inline void fail(std::string_view message) {
    std::cerr << "parser test failure: " << message << '\n';
    std::exit(1);
}

inline void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

inline std::unique_ptr<ast::CompUnit> parseProgram(
    std::string_view source, std::vector<Diagnostic>* diagnostics = nullptr) {
    TokenStream tokens(lex(source));
    parser::Parser parser(tokens);
    std::unique_ptr<ast::CompUnit> unit = parser.parseCompUnit();
    if (diagnostics != nullptr) {
        *diagnostics = parser.diagnostics();
    }
    return unit;
}

template <typename T>
const T& requireNode(const ast::Node& node) {
    const auto* result = dynamic_cast<const T*>(&node);
    if (result == nullptr) {
        std::ostringstream message;
        message << "expected AST node type " << typeid(T).name() << ", got "
                << typeid(node).name();
        fail(message.str());
    }
    return *result;
}

template <typename T>
const T& requireExpr(const ast::Expr& expr) {
    return requireNode<T>(expr);
}

template <typename T>
const T& requireStmt(const ast::Stmt& stmt) {
    return requireNode<T>(stmt);
}

template <typename T>
const T& requireDecl(const ast::Decl& decl) {
    return requireNode<T>(decl);
}

} // namespace toyc::test
