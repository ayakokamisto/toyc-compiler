#include "codegen/opt/IrOptimizer.h"
#include "ir/contract_ir_generator.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <variant>

namespace {

namespace c = toyc::codegen::contract;

void fail(std::string_view message) {
    std::cerr << "ir optimizer test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

// Build a single-function module with one entry block ending in `return ret`.
c::IRModule makeModule(std::vector<c::Instruction> instructions,
                       const std::string& retVreg,
                       std::vector<c::Param> params = {}) {
    c::IRModule module;
    module.functions.push_back({
        "main",
        c::Type::Int,
        std::move(params),
        {
            {"entry", std::move(instructions), c::ReturnInst{retVreg}},
        },
    });
    return module;
}

const c::Instruction& onlyInstruction(const c::IRModule& module) {
    const auto& insts = module.functions.front().basicBlocks.front().instructions;
    require(insts.size() == 1, "expected exactly one surviving instruction");
    return insts.front();
}

void verifies(const c::IRModule& module, std::string_view message) {
    std::vector<toyc::Diagnostic> diagnostics;
    require(toyc::ir::verifyContractModule(module, diagnostics), message);
}

void testConstantFolding() {
    // %a=6, %b=7, %r=%a*%b -> %r = 42
    c::IRModule module = makeModule(
        {c::ConstInst{"%a", 6}, c::ConstInst{"%b", 7}, c::MulInst{"%r", "%a", "%b"}}, "%r");
    toyc::codegen::IrOptimizer::optimize(module);
    verifies(module, "folded module verifies");

    const auto* folded = std::get_if<c::ConstInst>(&onlyInstruction(module));
    require(folded != nullptr && folded->dst == "%r" && folded->value == 42,
            "6 * 7 folds to a single ConstInst 42");
}

void testDivByZeroNotFolded() {
    // %a=5, %b=0, %r=%a/%b -> must NOT fold (UB / would crash)
    c::IRModule module = makeModule(
        {c::ConstInst{"%a", 5}, c::ConstInst{"%b", 0}, c::DivInst{"%r", "%a", "%b"}}, "%r");
    toyc::codegen::IrOptimizer::optimize(module);
    verifies(module, "div-by-zero module still verifies");

    bool hasDiv = false;
    for (const auto& inst : module.functions.front().basicBlocks.front().instructions) {
        if (std::holds_alternative<c::DivInst>(inst)) {
            hasDiv = true;
        }
    }
    require(hasDiv, "division by zero is left intact, not folded");
}

void testAlgebraicIdentities() {
    // param a; %z=0; %r = a + 0  ->  copy of a (then DCE leaves a single def chain)
    c::IRModule module = makeModule(
        {c::ConstInst{"%z", 0}, c::AddInst{"%r", "%a", "%z"}}, "%r", {{"a", "%a"}});
    toyc::codegen::IrOptimizer::optimize(module);
    verifies(module, "x+0 module verifies");

    const auto& insts = module.functions.front().basicBlocks.front().instructions;
    for (const auto& inst : insts) {
        require(!std::holds_alternative<c::AddInst>(inst), "x + 0 should not remain an AddInst");
    }
}

void testDeadCodeElimination() {
    // param a; %dead = a + a (never used); return a
    c::IRModule module =
        makeModule({c::AddInst{"%dead", "%a", "%a"}}, "%a", {{"a", "%a"}});
    toyc::codegen::IrOptimizer::optimize(module);
    verifies(module, "DCE module verifies");

    const auto& insts = module.functions.front().basicBlocks.front().instructions;
    require(insts.empty(), "dead pure instruction is removed");
}

void testCopyPropagation() {
    // param a; %b = copy a; %r = b + b ; return r  ->  %r = a + a
    c::IRModule module = makeModule(
        {c::CopyInst{"%b", "%a"}, c::AddInst{"%r", "%b", "%b"}}, "%r", {{"a", "%a"}});
    toyc::codegen::IrOptimizer::optimize(module);
    verifies(module, "copy-prop module verifies");

    bool addUsesParam = false;
    for (const auto& inst : module.functions.front().basicBlocks.front().instructions) {
        if (const auto* add = std::get_if<c::AddInst>(&inst)) {
            addUsesParam = add->src1 == "%a" && add->src2 == "%a";
        }
    }
    require(addUsesParam, "copy is propagated so the add reads the original param");
}

void testStoreGlobalNotEliminated() {
    // %v = 7; store @g = %v ; return %v  -> store must survive (side effect)
    c::IRModule module;
    module.globalVars.push_back({"@g", 0});
    module.functions.push_back({
        "main",
        c::Type::Int,
        {},
        {
            {"entry",
             {c::ConstInst{"%v", 7}, c::StoreGlobalInst{"@g", "%v"}},
             c::ReturnInst{"%v"}},
        },
    });
    toyc::codegen::IrOptimizer::optimize(module);

    bool hasStore = false;
    for (const auto& inst : module.functions.front().basicBlocks.front().instructions) {
        if (std::holds_alternative<c::StoreGlobalInst>(inst)) {
            hasStore = true;
        }
    }
    require(hasStore, "StoreGlobal has a side effect and must never be eliminated");
}

} // namespace

int main() {
    testConstantFolding();
    testDivByZeroNotFolded();
    testAlgebraicIdentities();
    testDeadCodeElimination();
    testCopyPropagation();
    testStoreGlobalNotEliminated();
    return 0;
}
