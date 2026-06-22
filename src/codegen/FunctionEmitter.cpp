#include "codegen/FunctionEmitter.h"

#include "codegen/CallingConvention.h"
#include "codegen/CodegenUtils.h"
#include "codegen/InstructionSelector.h"
#include "codegen/RiscvEmitter.h"
#include "codegen/StackFrame.h"
#include "codegen/VRegCollector.h"

#include <stdexcept>
#include <string>

namespace toyc::codegen {

FunctionEmitter::FunctionEmitter(RiscvEmitter& emitter, const BackendOptions& options)
    : emitter_(emitter), options_(options) {}

void FunctionEmitter::emit(const contract::IRFunction& function) {
    if (function.basicBlocks.empty()) {
        throw std::invalid_argument("function must contain at least one basic block");
    }

    StackFrame frame;
    VRegCollector::collectInto(function, frame);
    frame.finalize();

    const std::string functionEpilogueLabel = epilogueLabel(function.name);
    CallingConvention abi(emitter_, frame);
    InstructionSelector selector(emitter_, frame);

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
        for (const contract::Instruction& instruction : block.instructions) {
            selector.emit(instruction);
        }
        selector.emitTerminator(block.terminator, function.name, functionEpilogueLabel);
    }

    emitter_.label(functionEpilogueLabel);
    abi.emitEpilogue();
}

} // namespace toyc::codegen
