#pragma once
#include "ast.h"
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct FuncInfo {
    Type returnType;
    size_t paramCount;
};

struct GlobalSymbol {
    std::string sourceName;
    std::string asmLabel;
    int32_t initialValue;
    bool zeroInitialized;
    SourceLocation declaredAt;
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer();
    bool analyze(const Program& program);
    bool has_errors() const { return has_errors_; }
    void print_errors(std::ostream& os) const;
    const FuncInfo* lookup_function(const std::string& name) const;
private:
    struct Scope { std::unordered_set<std::string> names; };
    void push_scope(); void pop_scope();
    bool is_in_current_scope(const std::string& name) const;
    bool is_declared(const std::string& name) const;
    void declare(const std::string& name, const SourceLocation& loc);
    void analyze_function(const FunctionDef& func);
    void analyze_stmt(const Stmt& stmt);
    void analyze_compound(const CompoundStmt& stmt);
    void analyze_var_decl(const VarDeclStmt& stmt);
    void analyze_assign(const AssignStmt& stmt);
    void analyze_if(const IfStmt& stmt);
    void analyze_while(const WhileStmt& stmt);
    void analyze_return(const ReturnStmt& stmt);
    Type analyze_expr(const Expr& expr, bool allowVoidCallAsStatement = false);
    bool returns_on_all_paths(const Stmt& stmt) const;
    void error(const std::string& msg, const SourceLocation& loc);

    std::vector<Scope> scopes_;
    uint32_t loop_depth_ = 0;
    bool has_errors_ = false;
    std::unordered_map<std::string, GlobalSymbol> globals_;
    std::unordered_map<std::string, FuncInfo> functions_;
    std::optional<int32_t> evaluate_global_constant(const Expr& expr);
    bool eval_binary_op(int32_t& result, BinaryOp op, int32_t left, int32_t right);
    std::string current_func_;
    Type current_return_type_ = Type::Int;
    struct ErrorEntry { SourceLocation loc; std::string message; };
    std::vector<ErrorEntry> errors_;
};
