#include "sema/sema.h"
#include "sema/const_eval.h"
#include "sema/return_check.h"
#include "ast/ast.h"
#include "common/diagnostic.h"
#include "common/token.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace toyc::sema {
namespace {

// Parse a decimal integer literal string to int64_t.
// Returns nullopt on parse failure or overflow.
std::optional<std::int64_t> parseInt(const std::string& spelling) {
    if (spelling.empty()) {
        return std::nullopt;
    }
    char* end = nullptr;
    errno = 0;
    const std::int64_t value = std::strtoll(spelling.c_str(), &end, 10);
    if (errno == ERANGE) {
        return std::nullopt;
    }
    if (end != spelling.c_str() + spelling.size()) {
        return std::nullopt;
    }
    if (value < 0) {
        return std::nullopt;
    }
    return value;
}

} // namespace

// ---------------------------------------------------------------------------
// Scope management
// ---------------------------------------------------------------------------

Scope& Sema::pushScope(Scope* parent) {
    auto scope = std::make_unique<Scope>(parent);
    Scope* ptr = scope.get();
    ownedScopes_.push_back(std::move(scope));
    currentScope_ = ptr;
    return *ptr;
}

void Sema::popScope() {
    currentScope_ = currentScope_->parent();
}

// ---------------------------------------------------------------------------
// Diagnostic helpers
// ---------------------------------------------------------------------------

void Sema::error(SourceRange range, std::string message) {
    diags_->push_back({DiagnosticSeverity::Error, range, std::move(message)});
}

void Sema::warn(SourceRange range, std::string message) {
    diags_->push_back({DiagnosticSeverity::Warning, range, std::move(message)});
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

SemaResult Sema::analyze(const ast::CompUnit& unit) {
    SemaResult result;
    model_ = &result.model;
    diags_ = &result.diagnostics;

    // Create global scope (depth 0).
    auto globalScope = std::make_unique<Scope>(nullptr);
    currentScope_ = globalScope.get();
    ownedScopes_.push_back(std::move(globalScope));

    // Process declarations in source order.
    for (const auto& decl : unit.declarations) {
        analyzeDecl(*decl);
    }

    finalizeMainCheck();

    return result;
}

// ---------------------------------------------------------------------------
// Declaration visitors
// ---------------------------------------------------------------------------

void Sema::analyzeDecl(const ast::Decl& decl) {
    if (const auto* funcDef = dynamic_cast<const ast::FuncDef*>(&decl)) {
        analyzeFuncDef(*funcDef);
    } else if (const auto* varDecl = dynamic_cast<const ast::VarDecl*>(&decl)) {
        analyzeGlobalVarDecl(*varDecl);
    } else if (const auto* constDecl = dynamic_cast<const ast::ConstDecl*>(&decl)) {
        analyzeGlobalConstDecl(*constDecl);
    }
    // Param never appears at top level; ignore if it somehow does.
}

void Sema::analyzeGlobalVarDecl(const ast::VarDecl& decl) {
    const int depth = currentScope_->depth();

    // Analyze initializer.
    Type initType = analyzeExpr(*decl.initializer);
    if (initType == Type::Void) {
        error(decl.range,
              "cannot initialize variable '" + decl.name + "' with void expression");
    }

    // Check duplicate in current scope.
    auto sym = std::make_unique<Symbol>(
        SymbolKind::Variable, decl.name, Type::Int, &decl, depth);
    const Symbol* symPtr = model_->registerSymbol(std::move(sym));

    if (!currentScope_->insert(decl.name, symPtr)) {
        error(decl.range, "duplicate declaration '" + decl.name + "'");
        return;
    }

    model_->setDeclSymbol(&decl, symPtr);
}

void Sema::analyzeGlobalConstDecl(const ast::ConstDecl& decl) {
    const int depth = currentScope_->depth();

    // Analyze initializer first (needed for const_eval to resolve references).
    analyzeExpr(*decl.initializer);

    // Evaluate the constant initializer at compile time.
    auto constValue = evaluateConstExpr(*decl.initializer, *model_, *diags_);
    if (!constValue.has_value()) {
        error(decl.range,
              "constant '" + decl.name + "' must have a compile-time evaluable initializer");
        // Register the symbol anyway for error recovery.
        auto sym = std::make_unique<Symbol>(
            SymbolKind::Constant, decl.name, Type::Int, &decl, depth);
        const Symbol* symPtr = model_->registerSymbol(std::move(sym));
        currentScope_->insert(decl.name, symPtr);
        model_->setDeclSymbol(&decl, symPtr);
        return;
    }

    // Store the constant value on the initializer expression.
    model_->setConstantValue(decl.initializer.get(), *constValue);

    // Check duplicate in current scope.
    auto sym = std::make_unique<Symbol>(
        SymbolKind::Constant, decl.name, Type::Int, &decl, depth);
    const Symbol* symPtr = model_->registerSymbol(std::move(sym));

    if (!currentScope_->insert(decl.name, symPtr)) {
        error(decl.range, "duplicate declaration '" + decl.name + "'");
        return;
    }

    model_->setDeclSymbol(&decl, symPtr);
}

void Sema::analyzeLocalVarDecl(const ast::VarDecl& decl) {
    const int depth = currentScope_->depth();

    // Analyze initializer.
    Type initType = analyzeExpr(*decl.initializer);
    if (initType == Type::Void) {
        error(decl.range,
              "cannot initialize variable '" + decl.name + "' with void expression");
    }

    // Check duplicate in current scope.
    auto sym = std::make_unique<Symbol>(
        SymbolKind::Variable, decl.name, Type::Int, &decl, depth);
    const Symbol* symPtr = model_->registerSymbol(std::move(sym));

    if (!currentScope_->insert(decl.name, symPtr)) {
        error(decl.range, "duplicate declaration '" + decl.name + "'");
        return;
    }

    model_->setDeclSymbol(&decl, symPtr);
}

void Sema::analyzeLocalConstDecl(const ast::ConstDecl& decl) {
    const int depth = currentScope_->depth();

    // Analyze initializer first.
    analyzeExpr(*decl.initializer);

    // Evaluate the constant initializer at compile time.
    auto constValue = evaluateConstExpr(*decl.initializer, *model_, *diags_);
    if (!constValue.has_value()) {
        error(decl.range,
              "constant '" + decl.name + "' must have a compile-time evaluable initializer");
        auto sym = std::make_unique<Symbol>(
            SymbolKind::Constant, decl.name, Type::Int, &decl, depth);
        const Symbol* symPtr = model_->registerSymbol(std::move(sym));
        currentScope_->insert(decl.name, symPtr);
        model_->setDeclSymbol(&decl, symPtr);
        return;
    }

    model_->setConstantValue(decl.initializer.get(), *constValue);

    // Check duplicate in current scope.
    auto sym = std::make_unique<Symbol>(
        SymbolKind::Constant, decl.name, Type::Int, &decl, depth);
    const Symbol* symPtr = model_->registerSymbol(std::move(sym));

    if (!currentScope_->insert(decl.name, symPtr)) {
        error(decl.range, "duplicate declaration '" + decl.name + "'");
        return;
    }

    model_->setDeclSymbol(&decl, symPtr);
}

void Sema::analyzeParam(const ast::Param& param) {
    const int depth = currentScope_->depth();

    auto sym = std::make_unique<Symbol>(
        SymbolKind::Parameter, param.name, Type::Int, &param, depth);
    const Symbol* symPtr = model_->registerSymbol(std::move(sym));

    if (!currentScope_->insert(param.name, symPtr)) {
        error(param.range, "duplicate parameter '" + param.name + "'");
        return;
    }

    model_->setDeclSymbol(&param, symPtr);
}

void Sema::analyzeFuncDef(const ast::FuncDef& funcDef) {
    // Verify that the function is at global scope.
    if (currentScope_->depth() != 0) {
        error(funcDef.range, "function definition must be at global scope");
        return;
    }

    // Build parameter type list (all params are int).
    std::vector<Type> paramTypes(funcDef.parameters.size(), Type::Int);

    Type returnType = fromAstTypeKind(funcDef.returnType);

    // Register the function symbol in the global scope.
    // This must happen BEFORE analyzing the body so recursive calls work.
    auto funcSym = std::make_unique<FunctionSymbol>(
        funcDef.name, returnType, &funcDef, /*scopeDepth=*/0, std::move(paramTypes));
    const FunctionSymbol* funcSymPtr =
        model_->registerFunctionSymbol(std::move(funcSym));

    if (!currentScope_->insert(funcDef.name, funcSymPtr)) {
        error(funcDef.range,
              "duplicate declaration '" + funcDef.name + "'");
        return;
    }

    model_->setDeclSymbol(&funcDef, funcSymPtr);

    // Track main.
    if (funcDef.name == "main") {
        seenMain_ = true;
        // Validate main signature.
        if (funcDef.returnType != ast::TypeKind::Int) {
            error(funcDef.range, "'main' must return int");
        }
        if (!funcDef.parameters.empty()) {
            error(funcDef.range, "'main' must have no parameters");
        }
    }

    // Create function scope and analyze the body.
    pushScope(currentScope_);  // depth = 1

    // Register parameters in the function scope.
    for (const auto& param : funcDef.parameters) {
        analyzeParam(*param);
    }

    // Analyze the function body.
    Type savedReturnType = currentReturnType_;
    currentReturnType_ = returnType;
    analyzeBlockStmt(*funcDef.body);

    // Check return completeness for int functions.
    if (returnType == Type::Int) {
        if (!blockAlwaysReturns(*funcDef.body)) {
            error(funcDef.body->range,
                  "not all control paths return a value in int function '" +
                      funcDef.name + "'");
        }
    }

    currentReturnType_ = savedReturnType;
    popScope();
}

// ---------------------------------------------------------------------------
// Statement visitors
// ---------------------------------------------------------------------------

void Sema::analyzeStmt(const ast::Stmt& stmt) {
    if (const auto* s = dynamic_cast<const ast::BlockStmt*>(&stmt)) {
        analyzeBlockStmt(*s);
    } else if (const auto* s = dynamic_cast<const ast::EmptyStmt*>(&stmt)) {
        analyzeEmptyStmt(*s);
    } else if (const auto* s = dynamic_cast<const ast::ExprStmt*>(&stmt)) {
        analyzeExprStmt(*s);
    } else if (const auto* s = dynamic_cast<const ast::AssignStmt*>(&stmt)) {
        analyzeAssignStmt(*s);
    } else if (const auto* s = dynamic_cast<const ast::DeclStmt*>(&stmt)) {
        analyzeDeclStmt(*s);
    } else if (const auto* s = dynamic_cast<const ast::IfStmt*>(&stmt)) {
        analyzeIfStmt(*s);
    } else if (const auto* s = dynamic_cast<const ast::WhileStmt*>(&stmt)) {
        analyzeWhileStmt(*s);
    } else if (const auto* s = dynamic_cast<const ast::BreakStmt*>(&stmt)) {
        analyzeBreakStmt(*s);
    } else if (const auto* s = dynamic_cast<const ast::ContinueStmt*>(&stmt)) {
        analyzeContinueStmt(*s);
    } else if (const auto* s = dynamic_cast<const ast::ReturnStmt*>(&stmt)) {
        analyzeReturnStmt(*s);
    }
}

void Sema::analyzeBlockStmt(const ast::BlockStmt& block) {
    pushScope(currentScope_);
    for (const auto& stmt : block.statements) {
        analyzeStmt(*stmt);
    }
    popScope();
}

void Sema::analyzeEmptyStmt(const ast::EmptyStmt& /*stmt*/) {
    // Nothing to check.
}

void Sema::analyzeExprStmt(const ast::ExprStmt& stmt) {
    analyzeExpr(*stmt.expression);
    // Expression value is discarded; Void type is allowed here.
}

void Sema::analyzeAssignStmt(const ast::AssignStmt& stmt) {
    // Look up the target variable.
    const Symbol* sym = currentScope_->lookup(stmt.target);
    if (sym == nullptr) {
        error(stmt.range,
              "identifier '" + stmt.target + "' not declared");
        // Analyze the value for error recovery.
        analyzeExpr(*stmt.value);
        return;
    }

    // Reject assignment to constant.
    if (sym->kind == SymbolKind::Constant) {
        error(stmt.range,
              "cannot assign to constant '" + stmt.target + "'");
    }

    // Reject assignment to function.
    if (sym->kind == SymbolKind::Function) {
        error(stmt.range,
              "cannot assign to function '" + stmt.target + "'");
    }

    // Analyze the value expression.
    Type valueType = analyzeExpr(*stmt.value);
    if (valueType == Type::Void) {
        error(stmt.range,
              "cannot assign void expression to '" + stmt.target + "'");
    }
}

void Sema::analyzeDeclStmt(const ast::DeclStmt& stmt) {
    if (const auto* varDecl =
            dynamic_cast<const ast::VarDecl*>(stmt.declaration.get())) {
        analyzeLocalVarDecl(*varDecl);
    } else if (const auto* constDecl =
                   dynamic_cast<const ast::ConstDecl*>(stmt.declaration.get())) {
        analyzeLocalConstDecl(*constDecl);
    } else {
        // FuncDef or Param in a DeclStmt — parser shouldn't produce this.
        error(stmt.range, "invalid declaration in statement context");
    }
}

void Sema::analyzeIfStmt(const ast::IfStmt& stmt) {
    Type condType = analyzeExpr(*stmt.condition);
    if (condType == Type::Void) {
        error(stmt.condition->range,
              "void expression cannot be used as if condition");
    }

    analyzeStmt(*stmt.thenBranch);

    if (stmt.elseBranch) {
        analyzeStmt(*stmt.elseBranch);
    }
}

void Sema::analyzeWhileStmt(const ast::WhileStmt& stmt) {
    Type condType = analyzeExpr(*stmt.condition);
    if (condType == Type::Void) {
        error(stmt.condition->range,
              "void expression cannot be used as while condition");
    }

    ++loopDepth_;
    analyzeStmt(*stmt.body);
    --loopDepth_;
}

void Sema::analyzeBreakStmt(const ast::BreakStmt& stmt) {
    if (loopDepth_ == 0) {
        error(stmt.range, "'break' outside of loop");
    }
}

void Sema::analyzeContinueStmt(const ast::ContinueStmt& stmt) {
    if (loopDepth_ == 0) {
        error(stmt.range, "'continue' outside of loop");
    }
}

void Sema::analyzeReturnStmt(const ast::ReturnStmt& stmt) {
    if (currentReturnType_ == Type::Int) {
        if (!stmt.value) {
            error(stmt.range,
                  "int function must return a value");
        } else {
            Type valueType = analyzeExpr(*stmt.value);
            if (valueType == Type::Void) {
                error(stmt.range,
                      "cannot return void value from int function");
            }
        }
    } else {
        // currentReturnType_ == Type::Void
        if (stmt.value) {
            error(stmt.range,
                  "void function cannot return a value");
        }
    }
}

// ---------------------------------------------------------------------------
// Expression visitors
// ---------------------------------------------------------------------------

Type Sema::analyzeExpr(const ast::Expr& expr) {
    if (const auto* e = dynamic_cast<const ast::IntLiteralExpr*>(&expr)) {
        return analyzeIntLiteralExpr(*e);
    }
    if (const auto* e = dynamic_cast<const ast::DeclRefExpr*>(&expr)) {
        return analyzeDeclRefExpr(*e);
    }
    if (const auto* e = dynamic_cast<const ast::CallExpr*>(&expr)) {
        return analyzeCallExpr(*e);
    }
    if (const auto* e = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        return analyzeUnaryExpr(*e);
    }
    if (const auto* e = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        return analyzeBinaryExpr(*e);
    }
    // Unknown expression type — shouldn't happen.
    model_->setExprType(&expr, Type::Int);
    return Type::Int;
}

Type Sema::analyzeIntLiteralExpr(const ast::IntLiteralExpr& expr) {
    model_->setExprType(&expr, Type::Int);

    const auto parsed = parseInt(expr.spelling);
    if (!parsed.has_value()) {
        error(expr.range,
              "integer literal '" + expr.spelling + "' exceeds representable range");
        return Type::Int;
    }

    const std::int64_t value = *parsed;

    // 2147483648 is only valid as the operand of unary minus (-2147483648).
    // Any value strictly greater than 2147483648 is always invalid.
    if (value > 2147483648LL) {
        error(expr.range,
              "integer literal '" + expr.spelling + "' exceeds int32 range");
        return Type::Int;
    }

    // For 2147483648: defer the validity check. If it appears outside
    // UnaryExpr(Minus, ...), the user made an error. We record a diagnostic
    // here because the value cannot be represented as a positive int32.
    if (value > 2147483647LL) {
        // Exactly 2147483648 — only valid under negation.
        // We do NOT emit an error here because analyzeUnaryExpr handles the
        // valid case. If this literal appears in any other context, the
        // value is out of range; we catch that in const_eval or via the
        // parent expression.
    }

    return Type::Int;
}

Type Sema::analyzeDeclRefExpr(const ast::DeclRefExpr& expr) {
    const Symbol* sym = currentScope_->lookup(expr.name);
    if (sym == nullptr) {
        error(expr.range,
              "identifier '" + expr.name + "' not declared");
        model_->setExprType(&expr, Type::Int);
        return Type::Int;
    }

    model_->setBinding(&expr, sym);
    model_->setExprType(&expr, Type::Int);

    // Propagate constant value for constant references.
    // This ensures evaluateConstExpr can resolve DeclRefExpr → value.
    if (sym->kind == SymbolKind::Constant) {
        if (const auto* constDecl =
                dynamic_cast<const ast::ConstDecl*>(sym->decl)) {
            auto cv = model_->getConstantValue(*constDecl->initializer);
            if (cv.has_value()) {
                model_->setConstantValue(&expr, *cv);
            }
        }
    }

    return Type::Int;
}

Type Sema::analyzeCallExpr(const ast::CallExpr& expr) {
    // Resolve the callee name.
    const Symbol* sym = currentScope_->lookup(expr.callee);
    if (sym == nullptr) {
        error(expr.range,
              "function '" + expr.callee + "' not declared");
        model_->setExprType(&expr, Type::Int);
        // Analyze arguments for error recovery.
        for (const auto& arg : expr.arguments) {
            analyzeExpr(*arg);
        }
        return Type::Int;
    }

    if (sym->kind != SymbolKind::Function) {
        error(expr.range,
              "'" + expr.callee + "' is not a function");
        model_->setExprType(&expr, Type::Int);
        for (const auto& arg : expr.arguments) {
            analyzeExpr(*arg);
        }
        return Type::Int;
    }

    const auto* funcSym = static_cast<const FunctionSymbol*>(sym);
    model_->setCallee(&expr, funcSym);

    // Check argument count.
    if (expr.arguments.size() != funcSym->paramTypes.size()) {
        std::ostringstream msg;
        msg << "function '" << expr.callee << "' expects "
            << funcSym->paramTypes.size() << " argument"
            << (funcSym->paramTypes.size() != 1 ? "s" : "")
            << " but " << expr.arguments.size() << " were provided";
        error(expr.range, msg.str());
    }

    // Analyze arguments.
    for (const auto& arg : expr.arguments) {
        Type argType = analyzeExpr(*arg);
        if (argType == Type::Void) {
            error(arg->range,
                  "void expression cannot be used as function argument");
        }
    }

    Type returnType = funcSym->type;
    model_->setExprType(&expr, returnType);
    return returnType;
}

Type Sema::analyzeUnaryExpr(const ast::UnaryExpr& expr) {
    // Special case: -2147483648 (valid INT32_MIN representation).
    // The parser produces UnaryExpr(Minus, IntLiteralExpr("2147483648")).
    // We handle this before analyzing the operand to avoid a spurious
    // "integer literal exceeds int32 range" error.
    if (expr.op == TokenKind::Minus) {
        if (const auto* lit =
                dynamic_cast<const ast::IntLiteralExpr*>(expr.operand.get())) {
            const auto val = parseInt(lit->spelling);
            if (val.has_value() && *val == 2147483648LL) {
                // Valid: -2147483648 == INT32_MIN.
                model_->setExprType(lit, Type::Int);
                model_->setExprType(&expr, Type::Int);
                return Type::Int;
            }
        }
    }

    Type operandType = analyzeExpr(*expr.operand);

    // Check void operand for logical NOT.
    if (expr.op == TokenKind::Bang && operandType == Type::Void) {
        error(expr.range, "cannot apply '!' to void expression");
    }

    model_->setExprType(&expr, Type::Int);
    return Type::Int;
}

Type Sema::analyzeBinaryExpr(const ast::BinaryExpr& expr) {
    Type leftType = analyzeExpr(*expr.left);
    Type rightType = analyzeExpr(*expr.right);

    if (leftType == Type::Void) {
        error(expr.left->range,
              "void expression on left side of binary operator");
    }
    if (rightType == Type::Void) {
        error(expr.right->range,
              "void expression on right side of binary operator");
    }

    model_->setExprType(&expr, Type::Int);
    return Type::Int;
}

// ---------------------------------------------------------------------------
// Post-pass validation
// ---------------------------------------------------------------------------

void Sema::finalizeMainCheck() {
    if (!seenMain_) {
        error(SourceRange{},
              "program must contain a 'main' function returning int with no parameters");
    }
}

} // namespace toyc::sema
