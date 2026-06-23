#include "codegen/lower/FunctionEmitter.h"

#include "codegen/abi/CallingConvention.h"
#include "codegen/emit/CodegenUtils.h"
#include "codegen/emit/RiscvEmitter.h"
#include "codegen/frame/RegisterAllocator.h"
#include "codegen/lower/BranchFusionAnalysis.h"
#include "codegen/lower/InstructionSelector.h"

#include <stdexcept>
#include <string>
#include <variant>

namespace toyc::codegen {

FunctionEmitter::FunctionEmitter(RiscvEmitter& emitter, const BackendOptions& options)
    : emitter_(emitter), options_(options) {}

void FunctionEmitter::emit(const contract::IRFunction& function) {
    if (function.basicBlocks.empty()) {
        throw std::invalid_argument("function must contain at least one basic block");
    }

    const RegisterAllocation allocation =
        RegisterAllocator::allocate(function, options_.enableOpt);

    const std::string functionEpilogueLabel = epilogueLabel(function.name);
    CallingConvention abi(emitter_, allocation.frame, allocation.assignment);
    InstructionSelector selector(
        emitter_, allocation.frame, allocation.assignment, options_.enableOpt);

    if (options_.emitComment) {
        emitter_.comment("function " + function.name);
    }
    emitter_.global(function.name);
    emitter_.label(function.name);
    abi.emitPrologue();
    abi.emitParamLanding(function);

    for (const contract::BasicBlock& block : function.basicBlocks) {
        if (block.label != "entry") {
            emitter_.label(blockLabel(function.name, block.label));
        }
        selector.beginBasicBlock();

        const std::vector<contract::Instruction>& instructions = block.instructions;
        const auto* branch = std::get_if<contract::BranchInst>(&block.terminator);
        const bool canTryFusion =
            options_.enableOpt && branch != nullptr && !instructions.empty() &&
            !isBranchCondUsedInTargets(function, *branch, block.label);

        const std::size_t prefixCount =
            canTryFusion ? instructions.size() - 1 : instructions.size();
        for (std::size_t i = 0; i < prefixCount; ++i) {
            selector.emit(instructions[i]);
        }

        bool fused = false;
        if (canTryFusion) {
            fused = selector.tryEmitFusedCompareBranch(
                instructions.back(), *branch, function.name);
            if (!fused) {
                selector.emit(instructions.back());
            }
        }

        if (!fused) {
            selector.emitTerminator(block.terminator, function.name, functionEpilogueLabel);
        }
    }

    emitter_.label(functionEpilogueLabel);
    abi.emitEpilogue();
}

} // namespace toyc::codegen
