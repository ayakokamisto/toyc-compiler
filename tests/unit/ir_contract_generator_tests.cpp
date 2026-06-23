#include "common/diagnostic.h"
#include "common/token_stream.h"
#include "ir/contract_ir_generator.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/sema.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

namespace contract = toyc::codegen::contract;

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "ir contract generator test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

int errorCount(const std::vector<toyc::Diagnostic>& diagnostics) {
    int count = 0;
    for (const toyc::Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity == toyc::DiagnosticSeverity::Error) {
            ++count;
        }
    }
    return count;
}

std::unique_ptr<toyc::ast::CompUnit> parseProgram(std::string_view source) {
    toyc::TokenStream tokens(toyc::lex(std::string(source)));
    toyc::parser::Parser parser(tokens);
    auto unit = parser.parseCompUnit();
    require(!parser.hasError(), "parser should accept IR generator fixture");
    return unit;
}

contract::IRModule generateProgram(std::string_view source) {
    auto unit = parseProgram(source);

    toyc::sema::Sema sema;
    auto semaResult = sema.analyze(*unit);
    require(errorCount(semaResult.diagnostics) == 0,
            "sema should accept IR generator fixture");

    toyc::ir::ContractIRGenerator generator;
    contract::IRModule module = generator.generate(*unit, semaResult.model);
    require(errorCount(generator.diagnostics()) == 0,
            "IR generator should not emit diagnostics for valid fixture");

    std::vector<toyc::Diagnostic> verifierDiagnostics;
    require(toyc::ir::verifyContractModule(module, verifierDiagnostics),
            "generated contract IR should verify");
    require(errorCount(verifierDiagnostics) == 0,
            "contract verifier should not emit diagnostics for generated IR");

    return module;
}

const contract::IRFunction& findFunction(const contract::IRModule& module,
                                         std::string_view name) {
    for (const contract::IRFunction& function : module.functions) {
        if (function.name == name) {
            return function;
        }
    }
    fail("function not found");
}

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() &&
           text.substr(0, prefix.size()) == prefix;
}

const contract::BasicBlock& findBlock(const contract::IRFunction& function,
                                      std::string_view labelPrefix) {
    for (const contract::BasicBlock& block : function.basicBlocks) {
        if (startsWith(block.label, labelPrefix)) {
            return block;
        }
    }
    fail("block not found");
}

template <typename Inst>
bool hasInstruction(const contract::IRFunction& function) {
    for (const contract::BasicBlock& block : function.basicBlocks) {
        for (const contract::Instruction& instruction : block.instructions) {
            if (std::holds_alternative<Inst>(instruction)) {
                return true;
            }
        }
    }
    return false;
}

bool hasBlockPrefix(const contract::IRFunction& function,
                    std::string_view labelPrefix) {
    for (const contract::BasicBlock& block : function.basicBlocks) {
        if (startsWith(block.label, labelPrefix)) {
            return true;
        }
    }
    return false;
}

void testBasicReturn() {
    const contract::IRModule module = generateProgram("int main(){return 42;}");
    const contract::IRFunction& main = findFunction(module, "main");

    require(main.returnType == contract::Type::Int, "main returns int");
    require(main.basicBlocks.size() == 1, "basic return uses one block");
    require(std::holds_alternative<contract::ReturnInst>(
                main.basicBlocks.front().terminator),
            "entry terminates with return");
    require(hasInstruction<contract::ConstInst>(main), "literal lowers to CONST");
}

void testGlobalVariableLoadStore() {
    const contract::IRModule module =
        generateProgram("int value=5; int main(){value=value+3; return value;}");
    require(module.globalVars.size() == 1, "one global variable emitted");
    require(module.globalVars.front().name == "@value", "global name keeps @ prefix");
    require(module.globalVars.front().initValue == 5, "global initializer is folded");

    const contract::IRFunction& main = findFunction(module, "main");
    require(hasInstruction<contract::LoadGlobalInst>(main), "global read lowers to LOAD_GLOBAL");
    require(hasInstruction<contract::StoreGlobalInst>(main),
            "global write lowers to STORE_GLOBAL");
}

void testNestedShadowingUsesDistinctVRegs() {
    const contract::IRModule module = generateProgram(R"(
        int main() {
            int value = 1;
            {
                int value = 5;
                value = value + 1;
            }
            return value;
        }
    )");

    const contract::IRFunction& main = findFunction(module, "main");
    bool sawInnerValue = false;
    for (const contract::BasicBlock& block : main.basicBlocks) {
        for (const contract::Instruction& instruction : block.instructions) {
            if (const auto* copy = std::get_if<contract::CopyInst>(&instruction)) {
                sawInnerValue = sawInnerValue || copy->dst == "%value_1";
            }
        }
    }

    const auto& ret =
        std::get<contract::ReturnInst>(main.basicBlocks.back().terminator);
    require(ret.src.has_value() && *ret.src == "%value",
            "return after inner scope reads outer variable vreg");
    require(sawInnerValue, "shadowed local receives a distinct vreg");
}

void testFunctionAndVoidCalls() {
    const contract::IRModule module = generateProgram(R"(
        int add(int left, int right) { return left + right; }
        void touch() { return; }
        int main() { touch(); return add(3, 4); }
    )");

    const contract::IRFunction& add = findFunction(module, "add");
    require(add.params.size() == 2, "parameters are preserved");
    require(add.params[0].vreg == "%p0" && add.params[1].vreg == "%p1",
            "parameters receive stable vregs");

    const contract::IRFunction& main = findFunction(module, "main");
    require(hasInstruction<contract::CallVoidInst>(main),
            "void call lowers to CALL_VOID");
    require(hasInstruction<contract::CallInst>(main), "int call lowers to CALL");
}

void testWhileBreakContinueCfg() {
    const contract::IRModule module = generateProgram(R"(
        int main() {
            int index = 0;
            int sum = 0;
            while (index < 10) {
                index = index + 1;
                if (index % 2 == 0)
                    continue;
                if (index > 7)
                    break;
                sum = sum + index;
            }
            return sum;
        }
    )");

    const contract::IRFunction& main = findFunction(module, "main");
    require(hasBlockPrefix(main, "while_cond_"), "while condition block emitted");
    require(hasBlockPrefix(main, "while_body_"), "while body block emitted");
    require(hasBlockPrefix(main, "while_exit_"), "while exit block emitted");

    const contract::BasicBlock& continueBlock = findBlock(main, "if_then_");
    require(std::holds_alternative<contract::JumpInst>(continueBlock.terminator),
            "continue branch terminates with jump");
}

void testShortCircuitAndOrCfg() {
    const contract::IRModule andModule = generateProgram(R"(
        int main() {
            int zero = 0;
            if (zero != 0 && 10 / zero > 1)
                return 1;
            return 2;
        }
    )");
    const contract::IRFunction& andMain = findFunction(andModule, "main");
    require(hasBlockPrefix(andMain, "and_rhs_"), "&& emits rhs block");
    require(hasBlockPrefix(andMain, "and_false_"), "&& emits false block");
    require(hasBlockPrefix(andMain, "and_end_"), "&& emits merge block");
    require(hasInstruction<contract::DivInst>(andMain),
            "&& rhs expression still lowers inside rhs block");

    const contract::IRModule orModule = generateProgram(R"(
        int main() {
            int zero = 0;
            if (1 || 10 / zero > 1)
                return 3;
            return 4;
        }
    )");
    const contract::IRFunction& orMain = findFunction(orModule, "main");
    require(hasBlockPrefix(orMain, "or_true_"), "|| emits true block");
    require(hasBlockPrefix(orMain, "or_rhs_"), "|| emits rhs block");
    require(hasBlockPrefix(orMain, "or_end_"), "|| emits merge block");
}

void testVerifierRejectsBrokenModule() {
    contract::IRModule module;
    module.functions.push_back({"main",
                                contract::Type::Int,
                                {},
                                {{"entry", {}, contract::JumpInst{"missing"}}}});

    std::vector<toyc::Diagnostic> diagnostics;
    require(!toyc::ir::verifyContractModule(module, diagnostics),
            "verifier rejects bad branch target");
    require(errorCount(diagnostics) > 0, "verifier reports diagnostics");
}

} // namespace

int main() {
    testBasicReturn();
    testGlobalVariableLoadStore();
    testNestedShadowingUsesDistinctVRegs();
    testFunctionAndVoidCalls();
    testWhileBreakContinueCfg();
    testShortCircuitAndOrCfg();
    testVerifierRejectsBrokenModule();

    std::cout << "All IR contract generator tests passed.\n";
    return 0;
}
