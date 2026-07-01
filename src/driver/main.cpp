#include "toyc/backend/riscv_emitter.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/frontend/semantic_analyzer.h"
#include "toyc/ir/alloca_promotability.h"
#include "toyc/ir/control_flow_graph.h"
#include "toyc/ir/def_use.h"
#include "toyc/ir/dominator_tree.h"
#include "toyc/ir/ir_builder.h"
#include "toyc/ir/ir_printer.h"
#include "toyc/ir/ir_verifier.h"
#include "toyc/ir/mem2reg.h"
#include "toyc/ir/module.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

static std::unique_ptr<IRProgram> build_smoke_fixture() {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    LocalVar* x_local = fn->new_local("x", false);
    Constant* c3 = fn->new_constant(3);
    Constant* c5 = fn->new_constant(5);
    Constant* c1 = fn->new_constant(1);
    Constant* c0 = fn->new_constant(0);
    auto* then_lab = fn->new_label("if.then");
    auto* else_lab = fn->new_label("if.else");
    BasicBlock* entry = fn->entry_block();
    Temp* ti1 = fn->new_temp(Type::Int);
    Temp* ti2 = fn->new_temp(Type::Int);
    Temp* ti3 = fn->new_temp(Type::Int);
    Temp* ti4 = fn->new_temp(Type::Int);
    entry->add_instruction(std::make_unique<AllocaInstr>(x_local));
    entry->add_instruction(std::make_unique<LoadImmInstr>(ti1, c3));
    entry->add_instruction(std::make_unique<StoreInstr>(ti1, x_local));
    entry->add_instruction(std::make_unique<LoadInstr>(ti2, x_local));
    entry->add_instruction(std::make_unique<LoadImmInstr>(ti4, c5));
    entry->add_instruction(std::make_unique<CompareInstr>(ti3, CompareInstr::Predicate::Lt, ti2, ti4));
    entry->set_terminator(std::make_unique<CondBranchInstr>(ti3, then_lab, else_lab));
    auto then_bb = std::make_unique<BasicBlock>(then_lab);
    Temp* tj1 = fn->new_temp(Type::Int);
    Temp* tj2 = fn->new_temp(Type::Int);
    Temp* tj3 = fn->new_temp(Type::Int);
    then_bb->add_instruction(std::make_unique<LoadInstr>(tj1, x_local));
    then_bb->add_instruction(std::make_unique<LoadImmInstr>(tj3, c1));
    then_bb->add_instruction(std::make_unique<BinaryOpInstr>(tj2, BinaryOpInstr::Op::Add, tj1, tj3));
    then_bb->set_terminator(std::make_unique<ReturnInstr>(tj2));
    auto else_bb = std::make_unique<BasicBlock>(else_lab);
    Temp* ek1 = fn->new_temp(Type::Int);
    else_bb->add_instruction(std::make_unique<LoadImmInstr>(ek1, c0));
    else_bb->set_terminator(std::make_unique<ReturnInstr>(ek1));
    fn->add_block(std::move(then_bb));
    fn->add_block(std::move(else_bb));
    mod->add_function(std::move(fn));
    return std::make_unique<IRProgram>(std::move(mod));
}

static void print_indent(std::ostream& os, int depth) {
    for (int i = 0; i < depth; ++i) os << "  ";
}

static void print_ast(std::ostream& os, const Expr& expr, int depth = 0);
static void print_ast(std::ostream& os, const Expr& expr, int depth) {
    if (auto* int_lit = dynamic_cast<const IntLiteralExpr*>(&expr)) {
        print_indent(os, depth); os << "IntLiteral(" << int_lit->value << ")\n"; return;
    }
    if (auto* raw = dynamic_cast<const RawIntLiteralExpr*>(&expr)) {
        print_indent(os, depth); os << "RawIntLiteral(" << raw->magnitude << ")\n"; return;
    }
    if (auto* ident = dynamic_cast<const IdentifierExpr*>(&expr)) {
        print_indent(os, depth); os << "Identifier(" << ident->name << ")\n"; return;
    }
    if (auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
        const char* s = "";
        switch (unary->op) {
        case UnaryOp::Plus: s = "Plus"; break;
        case UnaryOp::Minus: s = "Minus"; break;
        case UnaryOp::Not: s = "Not"; break;
        }
        print_indent(os, depth); os << "UnaryExpr(" << s << ")\n";
        print_ast(os, *unary->operand, depth + 1); return;
    }
    if (auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
        const char* s = "";
        switch (binary->op) {
        case BinaryOp::Add: s = "Add"; break;
        case BinaryOp::Sub: s = "Sub"; break;
        case BinaryOp::Mul: s = "Mul"; break;
        case BinaryOp::Div: s = "Div"; break;
        case BinaryOp::Mod: s = "Mod"; break;
        case BinaryOp::Lt: s = "Lt"; break;
        case BinaryOp::Gt: s = "Gt"; break;
        case BinaryOp::Le: s = "Le"; break;
        case BinaryOp::Ge: s = "Ge"; break;
        case BinaryOp::Eq: s = "Eq"; break;
        case BinaryOp::Ne: s = "Ne"; break;
        case BinaryOp::And: s = "And"; break;
        case BinaryOp::Or: s = "Or"; break;
        }
        print_indent(os, depth); os << "BinaryExpr(" << s << ")\n";
        print_ast(os, *binary->left, depth + 1);
        print_ast(os, *binary->right, depth + 1); return;
    }
}

static void print_stmt_ast(std::ostream& os, const Stmt& stmt, int depth = 1) {
    if (auto* c = dynamic_cast<const CompoundStmt*>(&stmt)) {
        print_indent(os, depth); os << "CompoundStmt\n";
        for (const auto& s : c->statements) print_stmt_ast(os, *s, depth + 1);
    } else if (auto* d = dynamic_cast<const VarDeclStmt*>(&stmt)) {
        print_indent(os, depth); os << "VarDecl(" << d->name << ")\n";
        if (d->initializer) print_ast(os, *d->initializer, depth + 1);
    } else if (auto* a = dynamic_cast<const AssignStmt*>(&stmt)) {
        print_indent(os, depth); os << "Assign(" << a->name << ")\n";
        print_ast(os, *a->value, depth + 1);
    } else if (auto* i = dynamic_cast<const IfStmt*>(&stmt)) {
        print_indent(os, depth); os << "IfStmt\n";
        print_indent(os, depth + 1); os << "Condition:\n";
        print_ast(os, *i->condition, depth + 2);
        print_indent(os, depth + 1); os << "Then:\n";
        print_stmt_ast(os, *i->thenStmt, depth + 2);
        if (i->elseStmt) {
            print_indent(os, depth + 1); os << "Else:\n";
            print_stmt_ast(os, *i->elseStmt, depth + 2);
        }
    } else if (auto* w = dynamic_cast<const WhileStmt*>(&stmt)) {
        print_indent(os, depth); os << "WhileStmt\n";
        print_indent(os, depth + 1); os << "Condition:\n";
        print_ast(os, *w->condition, depth + 2);
        print_indent(os, depth + 1); os << "Body:\n";
        print_stmt_ast(os, *w->body, depth + 2);
    } else if (dynamic_cast<const BreakStmt*>(&stmt)) {
        print_indent(os, depth); os << "BreakStmt\n";
    } else if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        print_indent(os, depth); os << "ContinueStmt\n";
    } else if (auto* r = dynamic_cast<const ReturnStmt*>(&stmt)) {
        print_indent(os, depth); os << "ReturnStmt\n";
        if (r->value) print_ast(os, *r->value, depth + 1);
    } else if (auto* e = dynamic_cast<const ExprStmt*>(&stmt)) {
        print_indent(os, depth); os << "ExprStmt\n";
        print_ast(os, *e->expr, depth + 1);
    }
}

static void print_program_ast_old(std::ostream& os, const Program& program) {
    os << "Program\n";
    os << "  FunctionDef(" << program.functions[0].name << ")\n";
    print_stmt_ast(os, *program.functions[0].body, 2);
}

static void print_tokens(std::ostream& os, const std::vector<Token>& tokens) {
    for (const auto& tok : tokens) {
        os << tok.location.line << ":" << tok.location.column
           << " " << token_kind_name(tok.kind);
        if (!tok.lexeme.empty()) os << " '" << tok.lexeme << "'";
        if (tok.kind == TokenKind::IntegerLiteral) os << " (" << tok.int_value << ")";
        os << "\n";
    }
}

static void print_block_list(std::ostream& os, const std::vector<const BasicBlock*>& blocks) {
    os << "[";
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        if (i != 0) os << ", ";
        os << blocks[i]->label()->name();
    }
    os << "]";
}

static void dump_cfg(std::ostream& os, const IRProgram& ir) {
    for (const auto& fn : ir.module()->functions()) {
        ControlFlowGraph cfg(*fn);
        os << "function " << fn->name() << ":\n";
        for (const BasicBlock* block : cfg.blocks()) {
            os << "  " << block->label()->name()
               << ": reachable=" << (cfg.isReachable(*block) ? "yes" : "no");
            os << " preds="; print_block_list(os, cfg.predecessors(*block));
            os << " succs="; print_block_list(os, cfg.successors(*block));
            os << "\n";
        }
    }
}

static void dump_dom(std::ostream& os, const IRProgram& ir) {
    for (const auto& fn : ir.module()->functions()) {
        ControlFlowGraph cfg(*fn);
        DominatorTree dom(cfg);
        os << "function " << fn->name() << ":\n";
        for (const BasicBlock* block : cfg.blocks()) {
            const BasicBlock* idom = dom.immediateDominator(*block);
            os << "  " << block->label()->name()
               << ": idom=" << (idom ? idom->label()->name() : "<none>");
            os << " children="; print_block_list(os, dom.children(*block));
            os << " frontier="; print_block_list(os, dom.dominanceFrontier(*block));
            os << "\n";
        }
    }
}

static void dump_defuse(std::ostream& os, const IRProgram& ir) {
    for (const auto& fn : ir.module()->functions()) {
        DefUseIndex def_use(*fn);
        os << "function " << fn->name() << ":\n";
        for (const auto& block : fn->blocks()) {
            for (const Instruction* instruction : block->all_instrs()) {
                if (Value* result = instruction->result()) {
                    os << "  " << result->name() << ": def=" << IRPrinter::print(*instruction);
                    os << " uses=[";
                    const auto& uses = def_use.usesOf(*result);
                    for (std::size_t i = 0; i < uses.size(); ++i) {
                        if (i != 0) os << "; ";
                        os << IRPrinter::print(*uses[i].user) << " operand " << uses[i].operandIndex;
                    }
                    os << "]\n";
                }
            }
        }
    }
}

static void dump_promotable_allocas(std::ostream& os, const IRProgram& ir) {
    for (const auto& fn : ir.module()->functions()) {
        DefUseIndex def_use(*fn);
        AllocaPromotabilityAnalysis promotability(*fn, def_use);
        os << "function " << fn->name() << ":\n";
        os << "  entry alloca prefix:";
        for (const Value* value : promotability.entryAllocaPrefix()) {
            os << " " << value->name();
        }
        os << "\n";
        for (const Value* value : promotability.allocaAddresses()) {
            const auto& info = promotability.analyze(*value);
            os << "  " << value->name() << ": " << to_string(info.kind);
            if (info.kind != AllocaPromotionKind::Promotable) {
                os << " (" << info.reason << ")";
            }
            os << "\n";
        }
    }
}

static void dump_mem2reg(std::ostream& os, IRProgram& ir) {
    os << "before mem2reg:\n";
    os << IRPrinter::print(ir);
    os << "\n";

    std::vector<std::string> promoted;
    std::vector<std::string> skipped;
    std::vector<std::string> diagnostics;
    for (const auto& fn : ir.module()->functions()) {
        ControlFlowGraph cfg(*fn);
        DominatorTree dom(cfg);
        DefUseIndex def_use(*fn);
        AllocaPromotabilityAnalysis promotability(*fn, def_use);
        Mem2RegResult result = promoteMemToReg(*fn, cfg, dom, def_use, promotability);
        for (const Value* value : result.promotedAllocas) promoted.push_back(value->name());
        for (const Value* value : result.skippedAllocas) skipped.push_back(value->name());
        diagnostics.insert(diagnostics.end(), result.diagnostics.begin(), result.diagnostics.end());

        auto errors = verifyIR(*fn);
        auto ssa_errors = verifySSA(*fn);
        errors.insert(errors.end(), ssa_errors.begin(), ssa_errors.end());
        if (!errors.empty()) {
            for (const auto& error : errors) {
                diagnostics.push_back("verify: " + error.message);
            }
        }
    }

    os << "promoted allocas:\n";
    if (promoted.empty()) {
        os << "  <none>\n";
    } else {
        for (const auto& name : promoted) os << "  " << name << "\n";
    }
    os << "\n";

    os << "skipped allocas:\n";
    if (skipped.empty()) {
        os << "  <none>\n";
    } else {
        for (const auto& name : skipped) os << "  " << name << "\n";
    }
    os << "\n";

    if (!diagnostics.empty()) {
        os << "diagnostics:\n";
        for (const auto& diagnostic : diagnostics) os << "  " << diagnostic << "\n";
        os << "\n";
    }

    os << "after mem2reg:\n";
    os << IRPrinter::print(ir);
}

enum class Mode {
    DumpIrSmoke,
    DumpTokens,
    DumpAst,
    DumpIr,
    DumpCfg,
    DumpDom,
    DumpDefUse,
    DumpPromotableAllocas,
    DumpMem2Reg,
    Compile
};

int main(int argc, char* argv[]) {
    Mode mode = Mode::Compile;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dump-ir-smoke") == 0)      mode = Mode::DumpIrSmoke;
        else if (std::strcmp(argv[i], "--dump-tokens") == 0)   mode = Mode::DumpTokens;
        else if (std::strcmp(argv[i], "--dump-ast") == 0)      mode = Mode::DumpAst;
        else if (std::strcmp(argv[i], "--dump-ir") == 0)       mode = Mode::DumpIr;
        else if (std::strcmp(argv[i], "--dump-cfg") == 0)      mode = Mode::DumpCfg;
        else if (std::strcmp(argv[i], "--dump-dom") == 0)      mode = Mode::DumpDom;
        else if (std::strcmp(argv[i], "--dump-defuse") == 0)   mode = Mode::DumpDefUse;
        else if (std::strcmp(argv[i], "--dump-promotable-allocas") == 0)
            mode = Mode::DumpPromotableAllocas;
        else if (std::strcmp(argv[i], "--dump-mem2reg") == 0)
            mode = Mode::DumpMem2Reg;
        else if (std::strcmp(argv[i], "-opt") == 0) { /* no-op */ }
        else {
            std::cerr << argv[i] << ": error: unknown option\n";
            return 1;
        }
    }

    try {
        if (mode == Mode::DumpIrSmoke) {
            auto p = build_smoke_fixture();
            std::cout << IRPrinter::print(*p);
            return 0;
        }

        std::string source((std::istreambuf_iterator<char>(std::cin)),
                           std::istreambuf_iterator<char>());

        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (mode == Mode::DumpTokens) { print_tokens(std::cout, tokens); return 0; }

        Parser parser(tokens);
        Program program = parser.parse_program();
        if (mode == Mode::DumpAst) { print_program_ast_old(std::cout, program); return 0; }

        SemanticAnalyzer sema;
        if (!sema.analyze(program)) {
            sema.print_errors(std::cerr);
            return 1;
        }

        auto ir = IRBuilder::build(program);
        for (const auto& fn : ir->module()->functions()) {
            auto ir_errs = verifyIR(*fn);
            if (!ir_errs.empty()) {
                for (auto& e : ir_errs) std::cerr << "error: " << e.message << "\n";
                return 1;
            }
            DefUseIndex def_use(*fn);
            auto defuse_errs = verifyDefUseConsistency(*fn, def_use);
            if (!defuse_errs.empty()) {
                for (auto& e : defuse_errs) std::cerr << "error: " << e << "\n";
                return 1;
            }
        }

        if (mode == Mode::DumpIr) { std::cout << IRPrinter::print(*ir); return 0; }
        if (mode == Mode::DumpCfg) { dump_cfg(std::cout, *ir); return 0; }
        if (mode == Mode::DumpDom) { dump_dom(std::cout, *ir); return 0; }
        if (mode == Mode::DumpDefUse) { dump_defuse(std::cout, *ir); return 0; }
        if (mode == Mode::DumpPromotableAllocas) {
            dump_promotable_allocas(std::cout, *ir);
            return 0;
        }
        if (mode == Mode::DumpMem2Reg) {
            dump_mem2reg(std::cout, *ir);
            return 0;
        }

        for (const auto& fn : ir->module()->functions()) {
            auto p1_errs = verifyP1EmitterSupport(*fn);
            if (!p1_errs.empty()) {
                for (auto& e : p1_errs) std::cerr << "error: " << e.message << "\n";
                return 1;
            }
        }

        std::string asm_output = RiscvEmitter::emit(*ir);
        std::cout << asm_output;
        return 0;
    } catch (const LexError& e) {
        std::cerr << e.line << ":" << e.column << ": error: " << e.what() << "\n"; return 1;
    } catch (const ParseError& e) {
        std::cerr << e.what() << "\n"; return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n"; return 1;
    }
}
