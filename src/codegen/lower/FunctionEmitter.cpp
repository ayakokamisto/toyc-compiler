#include "codegen/lower/FunctionEmitter.h"

#include "codegen/abi/CallingConvention.h"
#include "codegen/emit/CodegenUtils.h"
#include "codegen/emit/RiscvEmitter.h"
#include "codegen/frame/RegisterAllocator.h"
#include "codegen/lower/BranchFusionAnalysis.h"
#include "codegen/lower/FoldableConsts.h"
#include "codegen/lower/InstructionSelector.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

namespace toyc::codegen {

FunctionEmitter::FunctionEmitter(RiscvEmitter& emitter, const BackendOptions& options)
    : emitter_(emitter), options_(options) {}

void FunctionEmitter::emit(const contract::IRFunction& function) {
    if (function.basicBlocks.empty()) {
        throw std::invalid_argument("function must contain at least one basic block");
    }

    std::unordered_map<std::string, std::int32_t> foldableConsts;
    if (options_.enableOpt) {
        foldableConsts = computeFoldableImmediateConsts(function);
    }

    const RegisterAllocation allocation =
        RegisterAllocator::allocate(function, options_.enableOpt, foldableConsts);

    const std::string functionEpilogueLabel = epilogueLabel(function.name);
    CallingConvention abi(emitter_, allocation.frame, allocation.assignment);
    InstructionSelector selector(
        emitter_, allocation.frame, allocation.assignment, options_.enableOpt, foldableConsts);

    if (options_.emitComment) {
        emitter_.comment("function " + function.name);
    }
    emitter_.global(function.name);
    emitter_.label(function.name);
    abi.emitPrologue();
    abi.emitParamLanding(function);
    // Entry-body label: sits after the prologue and parameter landing so
    // tail-recursion elimination can jump here (skipping stack-frame setup)
    // with freshly assigned parameter values.
    // Only emit when a TRE-rewritten back-edge (to __tailrec_entry) exists.
    // Ordinary jumps to "entry" (loop back-edges, etc.) are NOT affected.
    bool hasTailRecursionBackedge = false;
    for (const contract::BasicBlock& block : function.basicBlocks) {
        hasTailRecursionBackedge = std::visit(
            [](const auto& t) -> bool {
                using T = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<T, contract::JumpInst>) {
                    return t.targetLabel == "__tailrec_entry";
                } else if constexpr (std::is_same_v<T, contract::BranchInst>) {
                    return t.trueLabel == "__tailrec_entry" || t.falseLabel == "__tailrec_entry";
                } else {
                    return false;
                }
            },
            block.terminator);
        if (hasTailRecursionBackedge) {
            break;
        }
    }
    if (hasTailRecursionBackedge) {
        emitter_.label(blockLabel(function.name, "tail_entry"));
    }

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
