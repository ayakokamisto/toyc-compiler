#include "toyc/frontend/semantic_analyzer.h"
#include <sstream>

SemanticAnalyzer::SemanticAnalyzer() { push_scope(); }

bool SemanticAnalyzer::analyze(const Program& program) {
    // Phase S1: register all globals and function signatures
    for (const auto& g : program.globals) {
        if (functions_.count(g.name)) {
            std::ostringstream m; m << "conflicting global variable and function name '" << g.name << "'";
            error(m.str(), g.location);
        }
        if (globals_.count(g.name)) {
            std::ostringstream m; m << "redefinition of global variable '" << g.name << "'";
            error(m.str(), g.location);
        }
        if (!g.initializer) {
            std::ostringstream m; m << "global variable '" << g.name << "' requires an initializer";
            error(m.str(), g.location);
        }
        globals_[g.name] = {g.name, g.name, 0, false, g.location};
    }

    for (const auto& func : program.functions) {
        std::string n = func.name;
        if (globals_.count(n)) {
            std::ostringstream m; m << "conflicting function and global variable name '" << n << "'";
            error(m.str(), func.location);
        }
        if (functions_.count(n)) {
            std::ostringstream m; m << "redefinition of function '" << n << "'";
            error(m.str(), func.location);
            continue;
        }
        functions_[n] = {func.returnType, func.params.size()};
    }

    // Check main exists and has correct signature
    auto it = functions_.find("main");
    if (it == functions_.end()) {
        error("function 'main' not defined", SourceLocation{1,1});
        return false;
    }
    if (it->second.returnType != Type::Int) {
        error("function 'main' must return int", SourceLocation{1,1});
    }
    if (it->second.paramCount != 0) {
        error("function 'main' must have no parameters", SourceLocation{1,1});
    }

    // Phase S2: evaluate global initializers
    for (const auto& g : program.globals) {
        if (!has_errors_ && g.initializer) {
            auto val = evaluate_global_constant(*g.initializer);
            if (val.has_value()) {
                globals_[g.name].initialValue = val.value();
                globals_[g.name].zeroInitialized = (val.value() == 0);
            } else if (!has_errors_) {
                std::ostringstream m; m << "global initializer is not a constant integer expression";
                error(m.str(), g.location);
            }
        }
    }

    // Phase S3: analyze function bodies
    for (const auto& func : program.functions) {
        current_func_ = func.name;
        current_return_type_ = func.returnType;
        push_scope();
        for (const auto& p : func.params) declare(p.name, SourceLocation{1,1});
        if (func.body) {
            analyze_compound(*func.body);
            if (func.returnType == Type::Int && !returns_on_all_paths(*func.body)) {
                std::ostringstream m; m << "int function '" << func.name << "' may exit without return";
                error(m.str(), func.location);
            }
        }
        pop_scope();
    }
    return !has_errors_;
}

void SemanticAnalyzer::print_errors(std::ostream& os) const {
    for (const auto& e : errors_)
        os << e.loc.line << ":" << e.loc.column << ": error: " << e.message << "\n";
}

const FuncInfo* SemanticAnalyzer::lookup_function(const std::string& name) const {
    auto it = functions_.find(name);
    return it != functions_.end() ? &it->second : nullptr;
}

void SemanticAnalyzer::push_scope() { scopes_.emplace_back(); }
void SemanticAnalyzer::pop_scope() { if (!scopes_.empty()) scopes_.pop_back(); }
bool SemanticAnalyzer::is_in_current_scope(const std::string& name) const {
    return !scopes_.empty() && scopes_.back().names.count(name) > 0;
}
bool SemanticAnalyzer::is_declared(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
        if (it->names.count(name)) return true;
    return false;
}
void SemanticAnalyzer::declare(const std::string& name, const SourceLocation& loc) {
    if (is_in_current_scope(name)) {
        std::ostringstream m; m << "redefinition of variable '" << name << "'";
        error(m.str(), loc); return;
    }
    scopes_.back().names.insert(name);
}

void SemanticAnalyzer::analyze_compound(const CompoundStmt& stmt) {
    push_scope();
    for (const auto& s : stmt.statements) analyze_stmt(*s);
    pop_scope();
}
void SemanticAnalyzer::analyze_stmt(const Stmt& stmt) {
    if (auto* c = dynamic_cast<const CompoundStmt*>(&stmt)) { analyze_compound(*c); return; }
    if (auto* d = dynamic_cast<const VarDeclStmt*>(&stmt)) { analyze_var_decl(*d); return; }
    if (auto* a = dynamic_cast<const AssignStmt*>(&stmt)) { analyze_assign(*a); return; }
    if (auto* i = dynamic_cast<const IfStmt*>(&stmt)) { analyze_if(*i); return; }
    if (auto* w = dynamic_cast<const WhileStmt*>(&stmt)) { analyze_while(*w); return; }
    if (dynamic_cast<const BreakStmt*>(&stmt)) { if (!loop_depth_) error("break outside loop", {0,0}); return; }
    if (dynamic_cast<const ContinueStmt*>(&stmt)) { if (!loop_depth_) error("continue outside loop", {0,0}); return; }
    if (auto* r = dynamic_cast<const ReturnStmt*>(&stmt)) { analyze_return(*r); return; }
    if (auto* e = dynamic_cast<const ExprStmt*>(&stmt)) { analyze_expr(*e->expr, true); return; }
}
void SemanticAnalyzer::analyze_var_decl(const VarDeclStmt& stmt) {
    if (stmt.initializer) analyze_expr(*stmt.initializer);
    declare(stmt.name, stmt.location);
}
void SemanticAnalyzer::analyze_assign(const AssignStmt& stmt) {
    if (!is_declared(stmt.name) && !globals_.count(stmt.name)) {
        std::ostringstream m; m << "use of undeclared identifier '" << stmt.name << "'";
        error(m.str(), {0,0});
    }
    if (stmt.value) analyze_expr(*stmt.value);
}
void SemanticAnalyzer::analyze_if(const IfStmt& stmt) {
    analyze_expr(*stmt.condition);
    if (stmt.thenStmt) analyze_stmt(*stmt.thenStmt);
    if (stmt.elseStmt) analyze_stmt(*stmt.elseStmt);
}
void SemanticAnalyzer::analyze_while(const WhileStmt& stmt) {
    analyze_expr(*stmt.condition);
    loop_depth_++; if (stmt.body) analyze_stmt(*stmt.body); loop_depth_--;
}
void SemanticAnalyzer::analyze_return(const ReturnStmt& stmt) {
    if (stmt.value) {
        analyze_expr(*stmt.value);
    }
    if (current_return_type_ == Type::Void && stmt.value) {
        error("void function should not return a value", {0,0});
    }
}
Type SemanticAnalyzer::analyze_expr(const Expr& expr, bool allowVoidCallAsStatement) {
    if (dynamic_cast<const IntLiteralExpr*>(&expr) || dynamic_cast<const RawIntLiteralExpr*>(&expr)) {
        return Type::Int;
    }
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        if (!is_declared(id->name) && !globals_.count(id->name)) {
            auto* fi = lookup_function(id->name);
            if (!fi) {
                std::ostringstream m; m << "use of undeclared identifier '" << id->name << "'";
                error(m.str(), id->location);
            }
        }
        return Type::Int;
    }
    if (auto* u = dynamic_cast<const UnaryExpr*>(&expr)) {
        if (analyze_expr(*u->operand) == Type::Void) {
            error("void function call cannot be used as a value expression", {0,0});
        }
        return Type::Int;
    }
    if (auto* b = dynamic_cast<const BinaryExpr*>(&expr)) {
        if (analyze_expr(*b->left) == Type::Void) {
            error("void function call cannot be used as a value expression", {0,0});
        }
        if (analyze_expr(*b->right) == Type::Void) {
            error("void function call cannot be used as a value expression", {0,0});
        }
        return Type::Int;
    }
    if (auto* c = dynamic_cast<const CallExpr*>(&expr)) {
        auto* fi = lookup_function(c->callee);
        if (!fi) {
            std::ostringstream m; m << "call to undefined function '" << c->callee << "'";
            error(m.str(), {0,0});
        } else if (fi->paramCount != c->args.size()) {
            std::ostringstream m; m << "function '" << c->callee << "' expects "
                << fi->paramCount << " arguments but " << c->args.size() << " provided";
            error(m.str(), {0,0});
        }
        Type returnType = fi ? fi->returnType : Type::Int;
        const_cast<CallExpr*>(c)->return_type = returnType;
        for (const auto& arg : c->args) analyze_expr(*arg);
        if (returnType == Type::Void && !allowVoidCallAsStatement) {
            error("void function call cannot be used as a value expression", {0,0});
        }
        return returnType;
    }
    return Type::Int;
}

bool SemanticAnalyzer::returns_on_all_paths(const Stmt& stmt) const {
    if (dynamic_cast<const ReturnStmt*>(&stmt)) return true;
    if (auto* c = dynamic_cast<const CompoundStmt*>(&stmt)) {
        for (const auto& s : c->statements)
            if (returns_on_all_paths(*s)) return true;
        return false;
    }
    if (auto* i = dynamic_cast<const IfStmt*>(&stmt)) {
        bool t = i->thenStmt && returns_on_all_paths(*i->thenStmt);
        bool e = i->elseStmt && returns_on_all_paths(*i->elseStmt);
        return t && e;
    }
    return false;
}

void SemanticAnalyzer::error(const std::string& msg, const SourceLocation& loc) {
    has_errors_ = true; errors_.push_back({loc, msg});
}

std::optional<int32_t> SemanticAnalyzer::evaluate_global_constant(const Expr& expr) {
    if (auto* il = dynamic_cast<const IntLiteralExpr*>(&expr))
        return il->value;
    if (auto* u = dynamic_cast<const UnaryExpr*>(&expr)) {
        auto op = evaluate_global_constant(*u->operand);
        if (!op) return std::nullopt;
        switch (u->op) {
        case UnaryOp::Plus: return op;
        case UnaryOp::Minus: return -static_cast<int32_t>(op.value());
        case UnaryOp::Not: return (op.value() == 0) ? 1 : 0;
        }
    }
    if (auto* b = dynamic_cast<const BinaryExpr*>(&expr)) {
        if (b->op == BinaryOp::And) {
            auto l = evaluate_global_constant(*b->left);
            if (!l) return std::nullopt;
            if (l.value() == 0) return 0; // short-circuit
            auto r = evaluate_global_constant(*b->right);
            if (!r) return std::nullopt;
            return (r.value() != 0) ? 1 : 0;
        }
        if (b->op == BinaryOp::Or) {
            auto l = evaluate_global_constant(*b->left);
            if (!l) return std::nullopt;
            if (l.value() != 0) return 1; // short-circuit
            auto r = evaluate_global_constant(*b->right);
            if (!r) return std::nullopt;
            return (r.value() != 0) ? 1 : 0;
        }
        auto l = evaluate_global_constant(*b->left);
        auto r = evaluate_global_constant(*b->right);
        if (!l || !r) return std::nullopt;
        int32_t result = 0;
        if (eval_binary_op(result, b->op, l.value(), r.value()))
            return result;
        return std::nullopt;
    }
    // IdentifierExpr, CallExpr, RawIntLiteralExpr -> not a constant
    return std::nullopt;
}

bool SemanticAnalyzer::eval_binary_op(int32_t& result, BinaryOp op, int32_t left, int32_t right) {
    auto uleft = static_cast<uint32_t>(left);
    auto uright = static_cast<uint32_t>(right);
    switch (op) {
    case BinaryOp::Add: result = static_cast<int32_t>(uleft + uright); return true;
    case BinaryOp::Sub: result = static_cast<int32_t>(uleft - uright); return true;
    case BinaryOp::Mul: result = static_cast<int32_t>(uleft * uright); return true;
    case BinaryOp::Div:
        if (uright == 0) { error("division by zero in global initializer", {0,0}); return false; }
        result = static_cast<int32_t>(uleft / uright); return true;
    case BinaryOp::Mod:
        if (uright == 0) { error("division by zero in global initializer", {0,0}); return false; }
        result = static_cast<int32_t>(uleft % uright); return true;
    case BinaryOp::Lt: result = (left < right) ? 1 : 0; return true;
    case BinaryOp::Gt: result = (left > right) ? 1 : 0; return true;
    case BinaryOp::Le: result = (left <= right) ? 1 : 0; return true;
    case BinaryOp::Ge: result = (left >= right) ? 1 : 0; return true;
    case BinaryOp::Eq: result = (left == right) ? 1 : 0; return true;
    case BinaryOp::Ne: result = (left != right) ? 1 : 0; return true;
    default: return false;
    }
}
