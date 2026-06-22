#pragma once

#include "codegen/CallingConvention.h"
#include "codegen/ContractIR.h"
#include "codegen/StackFrame.h"

#include <string_view>

namespace toyc::codegen {

class RiscvEmitter;

class InstructionSelector {
public:
    InstructionSelector(RiscvEmitter& emitter, const StackFrame& frame);

    void emit(const contract::Instruction& instruction);
    void emitTerminator(const contract::Terminator& terminator,
                          std::string_view functionName,
                          std::string_view epilogueLabel);

private:
    void emitBinaryInputs(std::string_view src1, std::string_view src2);
    void emitBinaryOp(std::string_view dst,
                      std::string_view src1,
                      std::string_view src2,
                      std::string_view mnemonic);

    RiscvEmitter& emitter_;
    const StackFrame& frame_;
    CallingConvention abi_;
};

} // namespace toyc::codegen
