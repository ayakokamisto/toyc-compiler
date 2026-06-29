#include "ir/contract_ir_generator.h"

#include "ast/ast.h"
#include "common/token.h"
#include "sema/const_eval.h"
#include "sema/semantic_model.h"
#include "sema/symbol.h"
#include "sema/type.h"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace toyc::ir {
namespace {

namespace contract = toyc::codegen::contract;

contract::Type convertType(sema::Type type) {
    return type == sema::Type::Int ? contract::Type::Int : contract::Type::Void;
}

std::string globalName(std::string_view name) {
    return "@" + std::string(name);
}

bool isIdentifierChar(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}

std::string sanitizeVRegStem(std::string_view name) {
    std::string stem;
    stem.reserve(name.size());
    for (const char ch : name) {
        stem.push_back(isIdentifierChar(ch) ? ch : '_');
    }
    if (stem.empty()) {
        stem = "v";
    }
    return stem;
}

std::optional<std::int64_t> parseLiteral64(std::string_view spelling) {
    std::int64_t value = 0;
    const char* first = spelling.data();
    const char* last = spelling.data() + spelling.size();
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

class FunctionBuilder {
public:
    FunctionBuilder(const ast::FuncDef& astFunction,
                    const sema::SemanticModel& semanticModel,
                    const std::unordered_map<std::string, const sema::Symbol*>& globals,
                    std::vector<Diagnostic>& diagnostics)
        : astFunction_(astFunction), semanticModel_(semanticModel), globals_(globals),
          diagnostics_(diagnostics) {}

    contract::IRFunction build() {
        function_.name = astFunction_.name;
        function_.returnType =
            convertType(sema::fromAstTypeKind(astFunction_.returnType));

        pushScope();
        for (std::size_t index = 0; index < astFunction_.parameters.size(); ++index) {
            const ast::Param& param = *astFunction_.parameters[index];
            const std::string vreg = "%p" + std::to_string(index);
            usedVRegs_.insert(vreg);
            function_.params.push_back({param.name, vreg});

            if (const sema::Symbol* symbol = semanticModel_.getDeclSymbol(param)) {
                vregs_[symbol] = vreg;
                bindName(param.name, symbol);
            }
        }

        function_.basicBlocks.push_back({"entry", {}, contract::ReturnInst{}});
        currentBlock_ = 0;
        blockTerminated_.push_back(false);

        emitBlock(*astFunction_.body);
        if (!isCurrentTerminated()) {
            if (function_.returnType == contract::Type::Void) {
                terminate(contract::ReturnInst{std::nullopt});
            } else {
                const std::string zero = makeTemp();
                emit(contract::ConstInst{zero, 0});
                terminate(contract::ReturnInst{zero});
            }
        }

        popScope();
        return std::move(function_);
    }

private:
    struct LoopTargets {
        std::string continueLabel;
        std::string breakLabel;
    };

    void pushScope() { scopes_.push_back({}); }

    void popScope() { scopes_.pop_back(); }

    void bindName(const std::string& name, const sema::Symbol* symbol) {
        if (!scopes_.empty() && symbol != nullptr) {
            scopes_.back()[name] = symbol;
        }
    }

    const sema::Symbol* resolveName(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            const auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
        const auto global = globals_.find(name);
        return global != globals_.end() ? global->second : nullptr;
    }

    std::string makeTemp() { return "%t" + std::to_string(nextTemp_++); }

    std::string makeNamedVReg(std::string_view sourceName) {
        const std::string stem = "%" + sanitizeVRegStem(sourceName);
        if (usedVRegs_.insert(stem).second) {
            return stem;
        }

        for (std::size_t suffix = 1;; ++suffix) {
            const std::string candidate = stem + "_" + std::to_string(suffix);
            if (usedVRegs_.insert(candidate).second) {
                return candidate;
            }
        }
    }

    std::string vregForSymbol(const sema::Symbol& symbol) {
        const auto found = vregs_.find(&symbol);
        if (found != vregs_.end()) {
            return found->second;
        }

        std::string vreg = makeNamedVReg(symbol.name);
        vregs_.emplace(&symbol, vreg);
        return vreg;
    }

    std::string makeLabel(std::string_view stem) {
        return std::string(stem) + "_" + std::to_string(nextLabel_++);
    }

    void emit(contract::Instruction instruction) {
        ensureActiveBlock();
        function_.basicBlocks[currentBlock_].instructions.push_back(std::move(instruction));
    }

    void terminate(contract::Terminator terminator) {
        ensureActiveBlock();
        function_.basicBlocks[currentBlock_].terminator = std::move(terminator);
        blockTerminated_[currentBlock_] = true;
    }

    bool isCurrentTerminated() const {
        return currentBlock_ < blockTerminated_.size() && blockTerminated_[currentBlock_];
    }

    void ensureActiveBlock() {
        if (function_.basicBlocks.empty()) {
            function_.basicBlocks.push_back({"entry", {}, contract::ReturnInst{}});
            blockTerminated_.push_back(false);
            currentBlock_ = 0;
        } else if (isCurrentTerminated()) {
            appendBlock(makeLabel("unreachable"));
        }
    }

    void appendBlock(const std::string& label) {
        function_.basicBlocks.push_back({label, {}, contract::ReturnInst{}});
        blockTerminated_.push_back(false);
        currentBlock_ = function_.basicBlocks.size() - 1;
    }

    void switchToBlock(const std::string& label) {
        appendBlock(label);
    }

    void emitBlock(const ast::BlockStmt& block) {
        pushScope();
        for (const auto& statement : block.statements) {
            if (isCurrentTerminated()) {
                break;
            }
            emitStmt(*statement);
        }
        popScope();
    }

    void emitStmt(const ast::Stmt& statement) {
        ensureActiveBlock();

        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(&statement)) {
            emitBlock(*block);
        } else if (dynamic_cast<const ast::EmptyStmt*>(&statement) != nullptr) {
            return;
        } else if (const auto* exprStmt = dynamic_cast<const ast::ExprStmt*>(&statement)) {
            emitExpr(*exprStmt->expression);
        } else if (const auto* assign = dynamic_cast<const ast::AssignStmt*>(&statement)) {
            emitAssign(*assign);
        } else if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(&statement)) {
            emitDeclStmt(*declStmt);
        } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(&statement)) {
            emitIf(*ifStmt);
        } else if (const auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(&statement)) {
            emitWhile(*whileStmt);
        } else if (dynamic_cast<const ast::BreakStmt*>(&statement) != nullptr) {
            if (!loopStack_.empty()) {
                terminate(contract::JumpInst{loopStack_.back().breakLabel});
            }
        } else if (dynamic_cast<const ast::ContinueStmt*>(&statement) != nullptr) {
            if (!loopStack_.empty()) {
                terminate(contract::JumpInst{loopStack_.back().continueLabel});
            }
        } else if (const auto* returnStmt = dynamic_cast<const ast::ReturnStmt*>(&statement)) {
            emitReturn(*returnStmt);
        }
    }

    void emitAssign(const ast::AssignStmt& statement) {
        const std::string value = emitExpr(*statement.value);
        const sema::Symbol* target = resolveName(statement.target);
        if (target == nullptr) {
            diagnostics_.push_back({DiagnosticSeverity::Error, statement.targetRange,
                                    "IR generation could not resolve assignment target '" +
                                        statement.target + "'"});
            return;
        }

        if (target->scopeDepth == 0 && target->kind == sema::SymbolKind::Variable) {
            emit(contract::StoreGlobalInst{globalName(target->name), value});
            return;
        }

        emit(contract::CopyInst{vregForSymbol(*target), value});
    }

    void emitDeclStmt(const ast::DeclStmt& statement) {
        if (const auto* varDecl =
                dynamic_cast<const ast::VarDecl*>(statement.declaration.get())) {
            const sema::Symbol* symbol = semanticModel_.getDeclSymbol(*varDecl);
            bindName(varDecl->name, symbol);
            if (symbol == nullptr) {
                diagnostics_.push_back({DiagnosticSeverity::Error, varDecl->range,
                                        "IR generation could not resolve local variable '" +
                                            varDecl->name + "'"});
                return;
            }

            const std::string value = emitExpr(*varDecl->initializer);
            emit(contract::CopyInst{vregForSymbol(*symbol), value});
        } else if (const auto* constDecl =
                       dynamic_cast<const ast::ConstDecl*>(statement.declaration.get())) {
            bindName(constDecl->name, semanticModel_.getDeclSymbol(*constDecl));
        }
    }

    void emitIf(const ast::IfStmt& statement) {
        const std::string condition = emitExpr(*statement.condition);
        const std::string thenLabel = makeLabel("if_then");
        const std::string elseLabel =
            statement.elseBranch ? makeLabel("if_else") : makeLabel("if_end");
        const std::string endLabel =
            statement.elseBranch ? makeLabel("if_end") : elseLabel;

        terminate(contract::BranchInst{condition, thenLabel, elseLabel});

        switchToBlock(thenLabel);
        emitStmt(*statement.thenBranch);
        const bool thenTerminated = isCurrentTerminated();
        if (!thenTerminated) {
            terminate(contract::JumpInst{endLabel});
        }

        bool elseTerminated = false;
        if (statement.elseBranch) {
            switchToBlock(elseLabel);
            emitStmt(*statement.elseBranch);
            elseTerminated = isCurrentTerminated();
            if (!elseTerminated) {
                terminate(contract::JumpInst{endLabel});
            }
        }

        if (!thenTerminated || !elseTerminated || !statement.elseBranch) {
            switchToBlock(endLabel);
        }
    }

    void emitWhile(const ast::WhileStmt& statement) {
        const std::string condLabel = makeLabel("while_cond");
        const std::string bodyLabel = makeLabel("while_body");
        const std::string exitLabel = makeLabel("while_exit");

        terminate(contract::JumpInst{condLabel});

        switchToBlock(condLabel);
        const std::string condition = emitExpr(*statement.condition);
        terminate(contract::BranchInst{condition, bodyLabel, exitLabel});

        switchToBlock(bodyLabel);
        loopStack_.push_back({condLabel, exitLabel});
        emitStmt(*statement.body);
        loopStack_.pop_back();
        if (!isCurrentTerminated()) {
            terminate(contract::JumpInst{condLabel});
        }

        switchToBlock(exitLabel);
    }

    void emitReturn(const ast::ReturnStmt& statement) {
        if (statement.value) {
            terminate(contract::ReturnInst{emitExpr(*statement.value)});
        } else {
            terminate(contract::ReturnInst{std::nullopt});
        }
    }

    std::string emitExpr(const ast::Expr& expression) {
        if (const auto value = semanticModel_.getConstantValue(expression)) {
            return emitConst(*value);
        }

        if (const auto* literal = dynamic_cast<const ast::IntLiteralExpr*>(&expression)) {
            const auto parsed = parseLiteral64(literal->spelling);
            return emitConst(static_cast<std::int32_t>(parsed.value_or(0)));
        }
        if (const auto* ref = dynamic_cast<const ast::DeclRefExpr*>(&expression)) {
            return emitDeclRef(*ref);
        }
        if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expression)) {
            return emitCall(*call);
        }
        if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(&expression)) {
            return emitUnary(*unary);
        }
        if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(&expression)) {
            return emitBinary(*binary);
        }

        diagnostics_.push_back({DiagnosticSeverity::Error, expression.range,
                                "IR generation encountered an unknown expression"});
        return emitConst(0);
    }

    std::string emitConst(std::int32_t value) {
        const std::string dst = makeTemp();
        emit(contract::ConstInst{dst, value});
        return dst;
    }

    std::string emitDeclRef(const ast::DeclRefExpr& expression) {
        if (const sema::Symbol* symbol = semanticModel_.lookupBinding(expression)) {
            if (symbol->kind == sema::SymbolKind::Constant) {
                if (const auto* constDecl =
                        dynamic_cast<const ast::ConstDecl*>(symbol->decl)) {
                    if (const auto value =
                            semanticModel_.getConstantValue(*constDecl->initializer)) {
                        return emitConst(*value);
                    }
                }
            }
            if (symbol->scopeDepth == 0 && symbol->kind == sema::SymbolKind::Variable) {
                const std::string dst = makeTemp();
                emit(contract::LoadGlobalInst{dst, globalName(symbol->name)});
                return dst;
            }
            return vregForSymbol(*symbol);
        }

        diagnostics_.push_back({DiagnosticSeverity::Error, expression.range,
                                "IR generation could not resolve identifier '" +
                                    expression.name + "'"});
        return emitConst(0);
    }

    std::string emitCall(const ast::CallExpr& expression) {
        std::vector<std::string> args;
        args.reserve(expression.arguments.size());
        for (const auto& arg : expression.arguments) {
            args.push_back(emitExpr(*arg));
        }

        const sema::FunctionSymbol* callee = semanticModel_.lookupCallee(expression);
        const std::string functionName = callee != nullptr ? callee->name : expression.callee;
        const sema::Type returnType =
            callee != nullptr ? callee->type : semanticModel_.getExprType(expression);

        if (returnType == sema::Type::Void) {
            emit(contract::CallVoidInst{functionName, std::move(args)});
            return emitConst(0);
        }

        const std::string dst = makeTemp();
        emit(contract::CallInst{dst, functionName, std::move(args)});
        return dst;
    }

    std::string emitUnary(const ast::UnaryExpr& expression) {
        if (expression.op == TokenKind::Plus) {
            return emitExpr(*expression.operand);
        }

        if (expression.op == TokenKind::Minus) {
            if (const auto* literal =
                    dynamic_cast<const ast::IntLiteralExpr*>(expression.operand.get())) {
                if (const auto parsed = parseLiteral64(literal->spelling);
                    parsed.has_value() && *parsed == 2147483648LL) {
                    return emitConst(static_cast<std::int32_t>(-2147483647 - 1));
                }
            }

            const std::string src = emitExpr(*expression.operand);
            const std::string dst = makeTemp();
            emit(contract::NegInst{dst, src});
            return dst;
        }

        if (expression.op == TokenKind::Bang) {
            const std::string src = emitExpr(*expression.operand);
            const std::string dst = makeTemp();
            emit(contract::LNotInst{dst, src});
            return dst;
        }

        diagnostics_.push_back({DiagnosticSeverity::Error, expression.range,
                                "IR generation encountered an unsupported unary operator"});
        return emitConst(0);
    }

    std::string emitBinary(const ast::BinaryExpr& expression) {
        if (expression.op == TokenKind::AmpAmp) {
            return emitShortCircuitAnd(expression);
        }
        if (expression.op == TokenKind::PipePipe) {
            return emitShortCircuitOr(expression);
        }

        const std::string left = emitExpr(*expression.left);
        const std::string right = emitExpr(*expression.right);
        const std::string dst = makeTemp();

        switch (expression.op) {
        case TokenKind::Plus:
            emit(contract::AddInst{dst, left, right});
            break;
        case TokenKind::Minus:
            emit(contract::SubInst{dst, left, right});
            break;
        case TokenKind::Star:
            emit(contract::MulInst{dst, left, right});
            break;
        case TokenKind::Slash:
            emit(contract::DivInst{dst, left, right});
            break;
        case TokenKind::Percent:
            emit(contract::ModInst{dst, left, right});
            break;
        case TokenKind::EqualEqual:
            emit(contract::EqInst{dst, left, right});
            break;
        case TokenKind::BangEqual:
            emit(contract::NeInst{dst, left, right});
            break;
        case TokenKind::Less:
            emit(contract::LtInst{dst, left, right});
            break;
        case TokenKind::LessEqual:
            emit(contract::LeInst{dst, left, right});
            break;
        case TokenKind::Greater:
            emit(contract::GtInst{dst, left, right});
            break;
        case TokenKind::GreaterEqual:
            emit(contract::GeInst{dst, left, right});
            break;
        default:
            diagnostics_.push_back(
                {DiagnosticSeverity::Error, expression.range,
                 "IR generation encountered an unsupported binary operator"});
            emit(contract::ConstInst{dst, 0});
            break;
        }

        return dst;
    }

    std::string emitShortCircuitAnd(const ast::BinaryExpr& expression) {
        const std::string result = makeTemp();
        const std::string rhsLabel = makeLabel("and_rhs");
        const std::string trueLabel = makeLabel("and_true");
        const std::string falseLabel = makeLabel("and_false");
        const std::string endLabel = makeLabel("and_end");

        const std::string left = emitExpr(*expression.left);
        terminate(contract::BranchInst{left, rhsLabel, falseLabel});

        switchToBlock(rhsLabel);
        const std::string right = emitExpr(*expression.right);
        terminate(contract::BranchInst{right, trueLabel, falseLabel});

        switchToBlock(trueLabel);
        emit(contract::ConstInst{result, 1});
        terminate(contract::JumpInst{endLabel});

        switchToBlock(falseLabel);
        emit(contract::ConstInst{result, 0});
        terminate(contract::JumpInst{endLabel});

        switchToBlock(endLabel);
        return result;
    }

    std::string emitShortCircuitOr(const ast::BinaryExpr& expression) {
        const std::string result = makeTemp();
        const std::string trueLabel = makeLabel("or_true");
        const std::string rhsLabel = makeLabel("or_rhs");
        const std::string falseLabel = makeLabel("or_false");
        const std::string endLabel = makeLabel("or_end");

        const std::string left = emitExpr(*expression.left);
        terminate(contract::BranchInst{left, trueLabel, rhsLabel});

        switchToBlock(rhsLabel);
        const std::string right = emitExpr(*expression.right);
        terminate(contract::BranchInst{right, trueLabel, falseLabel});

        switchToBlock(trueLabel);
        emit(contract::ConstInst{result, 1});
        terminate(contract::JumpInst{endLabel});

        switchToBlock(falseLabel);
        emit(contract::ConstInst{result, 0});
        terminate(contract::JumpInst{endLabel});

        switchToBlock(endLabel);
        return result;
    }

    const ast::FuncDef& astFunction_;
    const sema::SemanticModel& semanticModel_;
    const std::unordered_map<std::string, const sema::Symbol*>& globals_;
    std::vector<Diagnostic>& diagnostics_;

    contract::IRFunction function_;
    std::vector<bool> blockTerminated_;
    std::size_t currentBlock_ = 0;

    std::vector<std::unordered_map<std::string, const sema::Symbol*>> scopes_;
    std::unordered_map<const sema::Symbol*, std::string> vregs_;
    std::unordered_set<std::string> usedVRegs_;
    std::vector<LoopTargets> loopStack_;

    std::size_t nextTemp_ = 0;
    std::size_t nextLabel_ = 0;
};

} // namespace

contract::IRModule ContractIRGenerator::generate(
    const ast::CompUnit& unit, const sema::SemanticModel& semanticModel) {
    diagnostics_.clear();

    contract::IRModule module;
    std::unordered_map<std::string, const sema::Symbol*> globals;

    for (const sema::Symbol* symbol : semanticModel.globalSymbols()) {
        if (symbol == nullptr) {
            continue;
        }

        globals[symbol->name] = symbol;
        if (symbol->kind != sema::SymbolKind::Variable) {
            continue;
        }

        std::int32_t initValue = 0;
        if (const auto* varDecl = dynamic_cast<const ast::VarDecl*>(symbol->decl)) {
            initValue =
                sema::evaluateConstExpr(*varDecl->initializer, semanticModel, diagnostics_)
                    .value_or(0);
        }
        module.globalVars.push_back({globalName(symbol->name), initValue});
    }

    for (const auto& declaration : unit.declarations) {
        const auto* function = dynamic_cast<const ast::FuncDef*>(declaration.get());
        if (function == nullptr) {
            continue;
        }

        FunctionBuilder builder(*function, semanticModel, globals, diagnostics_);
        module.functions.push_back(builder.build());
    }

    return module;
}

const std::vector<Diagnostic>& ContractIRGenerator::diagnostics() const noexcept {
    return diagnostics_;
}

namespace {

void addVerifierError(std::vector<Diagnostic>& diagnostics, std::string message) {
    diagnostics.push_back({DiagnosticSeverity::Error, SourceRange{}, std::move(message)});
}

void collectInstructionDefs(const contract::Instruction& instruction,
                            std::unordered_set<std::string>& defs) {
    std::visit(
        [&](const auto& inst) {
            using Inst = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Inst, contract::ConstInst> ||
                          std::is_same_v<Inst, contract::CopyInst> ||
                          std::is_same_v<Inst, contract::LoadGlobalInst> ||
                          std::is_same_v<Inst, contract::CallInst> ||
                          std::is_same_v<Inst, contract::AddInst> ||
                          std::is_same_v<Inst, contract::SubInst> ||
                          std::is_same_v<Inst, contract::MulInst> ||
                          std::is_same_v<Inst, contract::DivInst> ||
                          std::is_same_v<Inst, contract::ModInst> ||
                          std::is_same_v<Inst, contract::NegInst> ||
                          std::is_same_v<Inst, contract::EqInst> ||
                          std::is_same_v<Inst, contract::NeInst> ||
                          std::is_same_v<Inst, contract::LtInst> ||
                          std::is_same_v<Inst, contract::LeInst> ||
                          std::is_same_v<Inst, contract::GtInst> ||
                          std::is_same_v<Inst, contract::GeInst> ||
                          std::is_same_v<Inst, contract::LNotInst>) {
                defs.insert(inst.dst);
            }
        },
        instruction);
}

void collectInstructionUses(const contract::Instruction& instruction,
                            std::vector<std::string>& uses,
                            std::vector<std::string>& globals) {
    std::visit(
        [&](const auto& inst) {
            using Inst = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Inst, contract::CopyInst>) {
                uses.push_back(inst.src);
            } else if constexpr (std::is_same_v<Inst, contract::LoadGlobalInst>) {
                globals.push_back(inst.name);
            } else if constexpr (std::is_same_v<Inst, contract::StoreGlobalInst>) {
                globals.push_back(inst.name);
                uses.push_back(inst.src);
            } else if constexpr (std::is_same_v<Inst, contract::CallInst> ||
                                 std::is_same_v<Inst, contract::CallVoidInst>) {
                uses.insert(uses.end(), inst.args.begin(), inst.args.end());
            } else if constexpr (std::is_same_v<Inst, contract::AddInst> ||
                                 std::is_same_v<Inst, contract::SubInst> ||
                                 std::is_same_v<Inst, contract::MulInst> ||
                                 std::is_same_v<Inst, contract::DivInst> ||
                                 std::is_same_v<Inst, contract::ModInst> ||
                                 std::is_same_v<Inst, contract::EqInst> ||
                                 std::is_same_v<Inst, contract::NeInst> ||
                                 std::is_same_v<Inst, contract::LtInst> ||
                                 std::is_same_v<Inst, contract::LeInst> ||
                                 std::is_same_v<Inst, contract::GtInst> ||
                                 std::is_same_v<Inst, contract::GeInst>) {
                uses.push_back(inst.src1);
                uses.push_back(inst.src2);
            } else if constexpr (std::is_same_v<Inst, contract::NegInst> ||
                                 std::is_same_v<Inst, contract::LNotInst>) {
                uses.push_back(inst.src);
            }
        },
        instruction);
}

void collectTerminatorUses(const contract::Terminator& terminator,
                           std::vector<std::string>& uses) {
    std::visit(
        [&](const auto& term) {
            using Term = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<Term, contract::BranchInst>) {
                uses.push_back(term.cond);
            } else if constexpr (std::is_same_v<Term, contract::ReturnInst>) {
                if (term.src.has_value()) {
                    uses.push_back(*term.src);
                }
            }
        },
        terminator);
}

void collectTerminatorTargets(const contract::Terminator& terminator,
                              std::vector<std::string>& targets) {
    std::visit(
        [&](const auto& term) {
            using Term = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<Term, contract::JumpInst>) {
                targets.push_back(term.targetLabel);
            } else if constexpr (std::is_same_v<Term, contract::BranchInst>) {
                targets.push_back(term.trueLabel);
                targets.push_back(term.falseLabel);
            }
        },
        terminator);
}

bool isReturnCompatible(const contract::Terminator& terminator,
                        contract::Type returnType) {
    const auto* ret = std::get_if<contract::ReturnInst>(&terminator);
    if (ret == nullptr) {
        return true;
    }
    return returnType == contract::Type::Void ? !ret->src.has_value()
                                              : ret->src.has_value();
}

} // namespace

bool verifyContractModule(const contract::IRModule& module,
                          std::vector<Diagnostic>& diagnostics) {
    const std::size_t originalErrorCount = diagnostics.size();

    std::unordered_set<std::string> globalNames;
    for (const contract::GlobalObject& global : module.globalVars) {
        if (!globalNames.insert(global.name).second) {
            addVerifierError(diagnostics,
                             "duplicate global object '" + global.name + "'");
        }
    }

    for (const contract::IRFunction& function : module.functions) {
        if (function.basicBlocks.empty()) {
            addVerifierError(diagnostics,
                             "function '" + function.name + "' has no basic blocks");
            continue;
        }

        if (function.basicBlocks.front().label != "entry") {
            addVerifierError(diagnostics,
                             "function '" + function.name +
                                 "' must start with entry block");
        }

        std::unordered_map<std::string, std::size_t> blockIndexByLabel;
        std::unordered_set<std::string> defs;
        std::vector<std::string> uses;
        std::vector<std::string> referencedGlobals;

        for (const contract::Param& param : function.params) {
            defs.insert(param.vreg);
        }

        for (std::size_t index = 0; index < function.basicBlocks.size(); ++index) {
            const contract::BasicBlock& block = function.basicBlocks[index];
            if (!blockIndexByLabel.emplace(block.label, index).second) {
                addVerifierError(diagnostics,
                                 "duplicate block label '" + block.label +
                                     "' in function '" + function.name + "'");
            }

            for (const contract::Instruction& instruction : block.instructions) {
                collectInstructionDefs(instruction, defs);
                collectInstructionUses(instruction, uses, referencedGlobals);
            }
            collectTerminatorUses(block.terminator, uses);

            if (!isReturnCompatible(block.terminator, function.returnType)) {
                addVerifierError(diagnostics,
                                 "return terminator type mismatch in function '" +
                                     function.name + "'");
            }
        }

        for (const std::string& global : referencedGlobals) {
            if (globalNames.find(global) == globalNames.end()) {
                addVerifierError(diagnostics,
                                 "function '" + function.name +
                                     "' references unknown global '" + global + "'");
            }
        }

        for (const std::string& use : uses) {
            if (defs.find(use) == defs.end()) {
                addVerifierError(diagnostics,
                                 "function '" + function.name +
                                     "' uses undefined vreg '" + use + "'");
            }
        }

        for (const contract::BasicBlock& block : function.basicBlocks) {
            std::vector<std::string> targets;
            collectTerminatorTargets(block.terminator, targets);
            for (const std::string& target : targets) {
                if (blockIndexByLabel.find(target) == blockIndexByLabel.end()) {
                    addVerifierError(diagnostics,
                                     "function '" + function.name +
                                         "' jumps to unknown block '" + target + "'");
                }
            }
        }

        std::unordered_set<std::string> reachable;
        std::vector<std::string> worklist{"entry"};
        while (!worklist.empty()) {
            const std::string label = worklist.back();
            worklist.pop_back();
            if (!reachable.insert(label).second) {
                continue;
            }

            const auto blockIt = blockIndexByLabel.find(label);
            if (blockIt == blockIndexByLabel.end()) {
                continue;
            }

            std::vector<std::string> targets;
            collectTerminatorTargets(
                function.basicBlocks[blockIt->second].terminator, targets);
            worklist.insert(worklist.end(), targets.begin(), targets.end());
        }

        for (const contract::BasicBlock& block : function.basicBlocks) {
            if (reachable.find(block.label) == reachable.end()) {
                addVerifierError(diagnostics,
                                 "function '" + function.name +
                                     "' contains unreachable block '" + block.label +
                                     "'");
            }
        }
    }

    return diagnostics.size() == originalErrorCount;
}

} // namespace toyc::ir
