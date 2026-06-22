#pragma once

#include "codegen/abi/CallingConvention.h"
#include "codegen/ContractIR.h"
#include "codegen/frame/StackFrame.h"

#include <string_view>

namespace toyc::codegen {

class RiscvEmitter;

class InstructionSelector {
public:
    InstructionSelector(RiscvEmitter& emitter, const StackFrame& frame, bool enableOpt = false);

    void emit(const contract::Instruction& instruction);
    void emitTerminator(const contract::Terminator& terminator,
                        std::string_view functionName,
                        std::string_view epilogueLabel);

    [[nodiscard]] bool tryEmitFusedCompareBranch(const contract::Instruction& lastInstruction,
                                                 const contract::BranchInst& branch,
                                                 std::string_view functionName);

private:
    void emitBinaryInputs(std::string_view src1, std::string_view src2);
    void emitBinaryOp(std::string_view dst,
                      std::string_view src1,
                      std::string_view src2,
                      std::string_view mnemonic);
    void emitBranchTo(std::string_view functionName,
                      std::string_view trueLabel,
                      std::string_view falseLabel,
                      std::string_view takenMnemonic,
                      std::string_view condReg);

    RiscvEmitter& emitter_;
    const StackFrame& frame_;
    CallingConvention abi_;
    bool enableOpt_ = false;
};

} // namespace toyc::codegen
