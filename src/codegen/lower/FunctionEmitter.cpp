#include "codegen/lower/FunctionEmitter.h"

#include "codegen/abi/CallingConvention.h"
#include "codegen/emit/CodegenUtils.h"
#include "codegen/emit/RiscvEmitter.h"
#include "codegen/frame/RegisterAllocator.h"
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

    const StackFrame frame = RegisterAllocator::allocate(function);

    const std::string functionEpilogueLabel = epilogueLabel(function.name);
    CallingConvention abi(emitter_, frame);
    InstructionSelector selector(emitter_, frame, options_.enableOpt);

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

        const std::vector<contract::Instruction>& instructions = block.instructions;
        const auto* branch = std::get_if<contract::BranchInst>(&block.terminator);
        const bool canFuseBranch =
            options_.enableOpt && branch != nullptr && !instructions.empty();
        const bool fused =
            canFuseBranch &&
            selector.tryEmitFusedCompareBranch(instructions.back(), *branch, function.name);

        const std::size_t regularCount =
            fused ? instructions.size() - 1 : instructions.size();
        for (std::size_t i = 0; i < regularCount; ++i) {
            selector.emit(instructions[i]);
        }

        if (!fused) {
            selector.emitTerminator(block.terminator, function.name, functionEpilogueLabel);
        }
    }

    emitter_.label(functionEpilogueLabel);
    abi.emitEpilogue();
}

} // namespace toyc::codegen
