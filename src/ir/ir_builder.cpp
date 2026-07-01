#include "toyc/ir/ir_builder.h"
#include <sstream>
#include <stdexcept>
#include <optional>

namespace {
std::optional<int32_t> eval_module_constant(const Expr& expr);

bool eval_binary(int32_t& result, BinaryOp op, int32_t left, int32_t right) {
    auto uleft = static_cast<uint32_t>(left);
    auto uright = static_cast<uint32_t>(right);
    switch (op) {
    case BinaryOp::Add: result = static_cast<int32_t>(uleft + uright); return true;
    case BinaryOp::Sub: result = static_cast<int32_t>(uleft - uright); return true;
    case BinaryOp::Mul: result = static_cast<int32_t>(uleft * uright); return true;
    case BinaryOp::Div:
        if (uright == 0) return false;
        result = static_cast<int32_t>(uleft / uright); return true;
    case BinaryOp::Mod:
        if (uright == 0) return false;
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

std::optional<int32_t> eval_module_constant(const Expr& expr) {
    if (auto* il = dynamic_cast<const IntLiteralExpr*>(&expr)) return il->value;
    if (auto* u = dynamic_cast<const UnaryExpr*>(&expr)) {
        auto operand = eval_module_constant(*u->operand);
        if (!operand) return std::nullopt;
        switch (u->op) {
        case UnaryOp::Plus: return operand;
        case UnaryOp::Minus: return -static_cast<int32_t>(*operand);
        case UnaryOp::Not: return *operand == 0 ? int32_t{1} : int32_t{0};
        }
    }
    if (auto* b = dynamic_cast<const BinaryExpr*>(&expr)) {
        if (b->op == BinaryOp::And) {
            auto lhs = eval_module_constant(*b->left);
            if (!lhs) return std::nullopt;
            if (*lhs == 0) return int32_t{0};
            auto rhs = eval_module_constant(*b->right);
            if (!rhs) return std::nullopt;
            return *rhs != 0 ? int32_t{1} : int32_t{0};
        }
        if (b->op == BinaryOp::Or) {
            auto lhs = eval_module_constant(*b->left);
            if (!lhs) return std::nullopt;
            if (*lhs != 0) return int32_t{1};
            auto rhs = eval_module_constant(*b->right);
            if (!rhs) return std::nullopt;
            return *rhs != 0 ? int32_t{1} : int32_t{0};
        }
        auto lhs = eval_module_constant(*b->left);
        auto rhs = eval_module_constant(*b->right);
        if (!lhs || !rhs) return std::nullopt;
        int32_t result = 0;
        if (eval_binary(result, b->op, *lhs, *rhs)) return result;
    }
    return std::nullopt;
}
}

IRBuilder::IRBuilder() : module_(std::make_unique<Module>()), short_circuit_enabled_(true) {}

std::unique_ptr<IRProgram> IRBuilder::build(const Program& program) {
    IRBuilder builder;
    for (const auto& g : program.globals) {
        int32_t init_val = 0;
        if (g.initializer) {
            auto val = eval_module_constant(*g.initializer);
            if (!val) throw std::runtime_error("global initializer is not a constant integer expression");
            init_val = *val;
        }
        builder.module_->new_global_var(g.name, init_val);
    }
    for (const auto& func : program.functions)
        builder.emit_function(func);
    return std::make_unique<IRProgram>(std::move(builder.module_));
}

void IRBuilder::push_scope() { scopes_.emplace_back(); }
void IRBuilder::pop_scope() { if (!scopes_.empty()) scopes_.pop_back(); }

Value* IRBuilder::lookup_variable(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto f = it->find(name); if (f != it->end()) return f->second.address;
    }
    return nullptr;
}
void IRBuilder::declare_variable(const std::string& name, Value* addr, const SourceLocation& loc) {
    if (scopes_.empty()) push_scope();
    scopes_.back()[name] = {addr, loc};
}

void IRBuilder::emit_function(const FunctionDef& func) {
    fn_ = new Function(func.name, func.returnType, std::vector<LocalVar*>{});
    module_->add_function(std::unique_ptr<Function>(fn_));
    entry_block_ = fn_->entry_block();
    entry_alloca_index_ = 0;
    current_block_ = entry_block_;

    // Create entry allocas and parameter LocalVars for each function parameter.
    std::vector<LocalVar*> param_locals;
    for (size_t i = 0; i < func.params.size(); i++) {
        LocalVar* addr = fn_->new_local(func.params[i].name, true);
        auto alloca = std::make_unique<AllocaInstr>(addr);
        entry_block_->insert_instruction(entry_alloca_index_++, std::move(alloca));
        param_locals.push_back(addr);
    }
    // Replace the empty params with the actual param address locals
    fn_->set_parameters(param_locals);
    push_scope();
    for (size_t i = 0; i < param_locals.size(); i++)
        declare_variable(func.params[i].name, param_locals[i], SourceLocation{0,0});

    EmitFlow flow = func.body ? emit_compound(*func.body) : EmitFlow::FallsThrough;
    if (flow == EmitFlow::FallsThrough && current_block_ && !current_block_->is_terminated()) {
        Value* def = func.returnType == Type::Void ? nullptr : emit_load_imm(0);
        current_block_->set_terminator(std::make_unique<ReturnInstr>(def));
    }
    pop_scope();
    fn_ = nullptr; entry_block_ = nullptr; current_block_ = nullptr;
}

Value* IRBuilder::emit_expr(const Expr& expr) {
    if (auto* c = dynamic_cast<const CallExpr*>(&expr)) return emit_call(*c);
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        Value* addr = lookup_variable(id->name);
        if (!addr) {
            // Global variable: emit GlobalAddr + Load
            Value* gaddr = emit_global_addr(id->name);
            return emit_load(gaddr);
        }
        return emit_load(addr);
    }
    if (auto* il = dynamic_cast<const IntLiteralExpr*>(&expr)) return emit_load_imm(il->value);
    if (auto* u = dynamic_cast<const UnaryExpr*>(&expr)) return emit_unary(*u);
    if (auto* b = dynamic_cast<const BinaryExpr*>(&expr)) return emit_binary(*b);
    throw std::runtime_error("unknown expression type");
}

Value* IRBuilder::emit_load_imm(int32_t value) {
    Temp* t = fn_->new_temp(Type::Int);
    current_block_->add_instruction(std::make_unique<LoadImmInstr>(t, fn_->new_constant(value)));
    return t;
}

Value* IRBuilder::emit_unary(const UnaryExpr& expr) {
    Value* op = emit_expr(*expr.operand);
    if (expr.op == UnaryOp::Plus) return op;
    Temp* r = fn_->new_temp(Type::Int);
    current_block_->add_instruction(
        std::make_unique<UnaryOpInstr>(r,
            expr.op == UnaryOp::Minus ? UnaryOpInstr::Op::Neg : UnaryOpInstr::Op::Not, op));
    return r;
}

Value* IRBuilder::emit_binary(const BinaryExpr& expr) {
    if (expr.op == BinaryOp::And || expr.op == BinaryOp::Or) return emit_logical_value(expr);
    bool is_cmp = (expr.op >= BinaryOp::Lt && expr.op <= BinaryOp::Ne);
    Value* l = emit_expr(*expr.left); Value* r = emit_expr(*expr.right);
    Temp* t = fn_->new_temp(Type::Int);
    if (is_cmp) {
        CompareInstr::Predicate p;
        switch (expr.op) {
        case BinaryOp::Lt: p = CompareInstr::Predicate::Lt; break;
        case BinaryOp::Gt: p = CompareInstr::Predicate::Gt; break;
        case BinaryOp::Le: p = CompareInstr::Predicate::Le; break;
        case BinaryOp::Ge: p = CompareInstr::Predicate::Ge; break;
        case BinaryOp::Eq: p = CompareInstr::Predicate::Eq; break;
        case BinaryOp::Ne: p = CompareInstr::Predicate::Ne; break;
        default: return nullptr;
        }
        current_block_->add_instruction(std::make_unique<CompareInstr>(t, p, l, r));
    } else {
        BinaryOpInstr::Op aop;
        switch (expr.op) {
        case BinaryOp::Add: aop = BinaryOpInstr::Op::Add; break;
        case BinaryOp::Sub: aop = BinaryOpInstr::Op::Sub; break;
        case BinaryOp::Mul: aop = BinaryOpInstr::Op::Mul; break;
        case BinaryOp::Div: aop = BinaryOpInstr::Op::Div; break;
        case BinaryOp::Mod: aop = BinaryOpInstr::Op::Mod; break;
        default: throw std::runtime_error("unexpected binary op");
        }
        current_block_->add_instruction(std::make_unique<BinaryOpInstr>(t, aop, l, r));
    }
    return t;
}

Value* IRBuilder::emit_call(const CallExpr& expr) {
    if (expr.return_type == Type::Void) {
        std::vector<Value*> arg_vals;
        for (const auto& arg : expr.args) arg_vals.push_back(emit_expr(*arg));
        current_block_->add_instruction(
            std::make_unique<CallInstr>(nullptr, expr.callee, Type::Void, arg_vals));
        return nullptr;
    }
    std::vector<Value*> arg_vals;
    for (const auto& arg : expr.args) arg_vals.push_back(emit_expr(*arg));
    Temp* result = fn_->new_temp(Type::Int);
    current_block_->add_instruction(std::make_unique<CallInstr>(result, expr.callee, Type::Int, arg_vals));
    return result;
}

// Short-circuit (P1-B)
Value* IRBuilder::emit_logical_value(const BinaryExpr& expr) {
    Value* slot = create_entry_alloca("logic.slot");
    emit_store(emit_load_imm(expr.op == BinaryOp::And ? 0 : 1), slot);
    auto* rhs_b = append_block(expr.op == BinaryOp::And ? "land.rhs" : "lor.rhs");
    auto* merge_b = append_block(expr.op == BinaryOp::And ? "land.end" : "lor.end");
    if (expr.op == BinaryOp::And) emit_condition(*expr.left, rhs_b->label(), merge_b->label());
    else emit_condition(*expr.left, merge_b->label(), rhs_b->label());
    current_block_ = rhs_b;
    Value* rv = emit_expr(*expr.right); emit_store(normalize_bool(rv), slot);
    if (!current_block_->is_terminated())
        current_block_->set_terminator(std::make_unique<BranchInstr>(merge_b->label()));
    current_block_ = merge_b; return emit_load(slot);
}
void IRBuilder::emit_condition(const Expr& expr, Label* tt, Label* ft) {
    if (auto* b = dynamic_cast<const BinaryExpr*>(&expr)) {
        if (b->op == BinaryOp::And) { auto* r = append_block("land.rhs");
            emit_condition(*b->left, r->label(), ft); current_block_ = r;
            emit_condition(*b->right, tt, ft); return; }
        if (b->op == BinaryOp::Or) { auto* r = append_block("lor.rhs");
            emit_condition(*b->left, tt, r->label()); current_block_ = r;
            emit_condition(*b->right, tt, ft); return; }
    }
    if (auto* u = dynamic_cast<const UnaryExpr*>(&expr)) {
        if (u->op == UnaryOp::Not) { emit_condition(*u->operand, ft, tt); return; }
    }
    if (auto* il = dynamic_cast<const IntLiteralExpr*>(&expr)) {
        current_block_->set_terminator(std::make_unique<BranchInstr>(il->value ? tt : ft)); return;
    }
    Value* v = emit_expr(expr); Temp* c = fn_->new_temp(Type::Int);
    current_block_->add_instruction(std::make_unique<CompareInstr>(c, CompareInstr::Predicate::Ne, v, fn_->new_constant(0)));
    current_block_->set_terminator(std::make_unique<CondBranchInstr>(c, tt, ft));
}
Value* IRBuilder::normalize_bool(Value* val) {
    Temp* r = fn_->new_temp(Type::Int);
    current_block_->add_instruction(std::make_unique<CompareInstr>(r, CompareInstr::Predicate::Ne, val, fn_->new_constant(0)));
    return r;
}

// Statements (P2-A)
EmitFlow IRBuilder::emit_stmt(const Stmt& stmt) {
    if (auto* c = dynamic_cast<const CompoundStmt*>(&stmt)) return emit_compound(*c);
    if (auto* d = dynamic_cast<const VarDeclStmt*>(&stmt)) return emit_var_decl(*d);
    if (auto* a = dynamic_cast<const AssignStmt*>(&stmt)) return emit_assign(*a);
    if (auto* i = dynamic_cast<const IfStmt*>(&stmt)) return emit_if(*i);
    if (auto* w = dynamic_cast<const WhileStmt*>(&stmt)) return emit_while(*w);
    if (dynamic_cast<const BreakStmt*>(&stmt)) return emit_break();
    if (dynamic_cast<const ContinueStmt*>(&stmt)) return emit_continue();
    if (auto* r = dynamic_cast<const ReturnStmt*>(&stmt)) return emit_return(*r);
    if (auto* e = dynamic_cast<const ExprStmt*>(&stmt)) return emit_expr_stmt(*e);
    return EmitFlow::FallsThrough;
}
EmitFlow IRBuilder::emit_compound(const CompoundStmt& stmt) {
    push_scope(); EmitFlow flow = EmitFlow::FallsThrough;
    for (const auto& s : stmt.statements) {
        if (flow == EmitFlow::Terminates) continue;
        flow = emit_stmt(*s);
    } pop_scope(); return flow;
}
EmitFlow IRBuilder::emit_var_decl(const VarDeclStmt& stmt) {
    Value* addr = create_entry_alloca(stmt.name);
    declare_variable(stmt.name, addr, stmt.location);
    if (stmt.initializer && current_block_ && !current_block_->is_terminated())
        emit_store(emit_expr(*stmt.initializer), addr);
    return EmitFlow::FallsThrough;
}
EmitFlow IRBuilder::emit_assign(const AssignStmt& stmt) {
    Value* addr = lookup_variable(stmt.name);
    if (!addr) {
        // Global assignment
        addr = emit_global_addr(stmt.name);
    }
    if (current_block_ && !current_block_->is_terminated()) emit_store(emit_expr(*stmt.value), addr);
    return EmitFlow::FallsThrough;
}
EmitFlow IRBuilder::emit_if(const IfStmt& stmt) {
    auto* then_b = append_block("if.then"); BasicBlock* else_b = nullptr; BasicBlock* merge_b = nullptr;
    if (stmt.elseStmt) { else_b = append_block("if.else");
        emit_condition(*stmt.condition, then_b->label(), else_b->label()); }
    else { merge_b = append_block("if.end");
        emit_condition(*stmt.condition, then_b->label(), merge_b->label()); }
    current_block_ = then_b; EmitFlow then_f = emit_stmt(*stmt.thenStmt);
    if (then_f == EmitFlow::FallsThrough && !current_block_->is_terminated()) {
        if (!merge_b) merge_b = append_block("if.end");
        current_block_->set_terminator(std::make_unique<BranchInstr>(merge_b->label()));
    }
    EmitFlow else_f = EmitFlow::Terminates;
    if (stmt.elseStmt) { current_block_ = else_b; else_f = emit_stmt(*stmt.elseStmt);
        if (else_f == EmitFlow::FallsThrough && !current_block_->is_terminated()) {
            if (!merge_b) merge_b = append_block("if.end");
            current_block_->set_terminator(std::make_unique<BranchInstr>(merge_b->label())); }
        }
    else else_f = EmitFlow::FallsThrough;
    if (then_f == EmitFlow::Terminates && else_f == EmitFlow::Terminates) {
        current_block_ = nullptr;
        return EmitFlow::Terminates;
    }
    current_block_ = merge_b; return EmitFlow::FallsThrough;
}
EmitFlow IRBuilder::emit_while(const WhileStmt& stmt) {
    auto* cond_b = append_block("while.cond"); auto* body_b = append_block("while.body"); auto* exit_b = append_block("while.end");
    if (current_block_ && !current_block_->is_terminated())
        current_block_->set_terminator(std::make_unique<BranchInstr>(cond_b->label()));
    current_block_ = cond_b; emit_condition(*stmt.condition, body_b->label(), exit_b->label());
    current_block_ = body_b; loop_stack_.push_back({exit_b, cond_b});
    EmitFlow body_f = emit_stmt(*stmt.body); loop_stack_.pop_back();
    if (body_f == EmitFlow::FallsThrough && current_block_ && !current_block_->is_terminated())
        current_block_->set_terminator(std::make_unique<BranchInstr>(cond_b->label()));
    current_block_ = exit_b; return EmitFlow::FallsThrough;
}
EmitFlow IRBuilder::emit_break() {
    if (!loop_stack_.empty() && current_block_ && !current_block_->is_terminated())
        current_block_->set_terminator(std::make_unique<BranchInstr>(loop_stack_.back().break_target->label()));
    return EmitFlow::Terminates;
}
EmitFlow IRBuilder::emit_continue() {
    if (!loop_stack_.empty() && current_block_ && !current_block_->is_terminated())
        current_block_->set_terminator(std::make_unique<BranchInstr>(loop_stack_.back().continue_target->label()));
    return EmitFlow::Terminates;
}
EmitFlow IRBuilder::emit_return(const ReturnStmt& stmt) {
    Value* val = stmt.value ? emit_expr(*stmt.value) : nullptr;
    if (current_block_ && !current_block_->is_terminated())
        current_block_->set_terminator(std::make_unique<ReturnInstr>(val));
    return EmitFlow::Terminates;
}
EmitFlow IRBuilder::emit_expr_stmt(const ExprStmt& stmt) { emit_expr(*stmt.expr); return EmitFlow::FallsThrough; }

Value* IRBuilder::create_entry_alloca(const std::string& hint) {
    LocalVar* l = fn_->new_local(hint, false);
    auto a = std::make_unique<AllocaInstr>(l); Value* addr = a->result();
    entry_block_->insert_instruction(entry_alloca_index_++, std::move(a));
    return addr;
}
Value* IRBuilder::emit_load(Value* addr) {
    Temp* r = fn_->new_temp(Type::Int);
    current_block_->add_instruction(std::make_unique<LoadInstr>(r, addr));
    return r;
}
void IRBuilder::emit_store(Value* val, Value* addr) {
    current_block_->add_instruction(std::make_unique<StoreInstr>(val, addr));
}
BasicBlock* IRBuilder::append_block(const std::string& hint) {
    auto* l = fn_->new_label(hint); auto b = std::make_unique<BasicBlock>(l);
    auto* raw = b.get(); fn_->add_block(std::move(b)); return raw;
}
Value* IRBuilder::emit_global_addr(const std::string& name) {
    // Find the GlobalVar in the module
    GlobalVar* gv = nullptr;
    for (auto* g : module_->globals()) {
        if (g->symbol_name() == name) { gv = g; break; }
    }
    if (!gv) throw std::runtime_error("global variable '" + name + "' not found");
    Temp* addr = fn_->new_temp(Type::Int);
    current_block_->add_instruction(std::make_unique<GlobalAddrInstr>(addr, gv));
    return addr;
}

void IRBuilder::emit(std::unique_ptr<Instr> instr) { current_block_->add_instruction(std::move(instr)); }
