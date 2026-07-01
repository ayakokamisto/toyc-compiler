#pragma once
#include "toyc/frontend/ast.h"
#include "toyc/frontend/token.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

enum class EmitFlow : uint8_t { FallsThrough, Terminates };

class IRBuilder {
public:
    static std::unique_ptr<IRProgram> build(const Program& program);
private:
    IRBuilder();
    void emit_function(const FunctionDef& func);
    Value* emit_expr(const Expr& expr);
    Value* emit_load_imm(int32_t value);
    Value* emit_unary(const UnaryExpr& expr);
    Value* emit_binary(const BinaryExpr& expr);
    Value* emit_call(const CallExpr& expr);
    Value* emit_logical_value(const BinaryExpr& expr);
    void emit_condition(const Expr& expr, Label* tt, Label* ft);
    Value* normalize_bool(Value* val);
    EmitFlow emit_stmt(const Stmt& stmt);
    EmitFlow emit_compound(const CompoundStmt& stmt);
    EmitFlow emit_var_decl(const VarDeclStmt& stmt);
    EmitFlow emit_assign(const AssignStmt& stmt);
    EmitFlow emit_if(const IfStmt& stmt);
    EmitFlow emit_while(const WhileStmt& stmt);
    EmitFlow emit_break();
    EmitFlow emit_continue();
    EmitFlow emit_return(const ReturnStmt& stmt);
    EmitFlow emit_expr_stmt(const ExprStmt& stmt);
    Value* create_entry_alloca(const std::string& hint);
    Value* emit_global_addr(const std::string& name);
    Value* emit_load(Value* addr);
    void emit_store(Value* val, Value* addr);
    BasicBlock* append_block(const std::string& hint);
    void emit(std::unique_ptr<Instr> instr);

    struct LocalBinding { Value* address; SourceLocation declaredAt; };
    using ScopeMap = std::unordered_map<std::string, LocalBinding>;
    std::vector<ScopeMap> scopes_;
    void push_scope(); void pop_scope();
    Value* lookup_variable(const std::string& name) const;
    void declare_variable(const std::string& name, Value* addr, const SourceLocation& loc);

    struct LoopContext { BasicBlock* break_target; BasicBlock* continue_target; };
    std::vector<LoopContext> loop_stack_;

    std::unique_ptr<Module> module_;
    Function* fn_ = nullptr;
    BasicBlock* entry_block_ = nullptr;
    size_t entry_alloca_index_ = 0;
    BasicBlock* current_block_ = nullptr;
    bool short_circuit_enabled_ = true;
};
