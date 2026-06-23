#pragma once

#include "codegen/ContractIR.h"

#include <functional>
#include <string_view>

namespace toyc::codegen {

using VRegOperandFn = std::function<void(std::string_view vreg)>;

void forEachInstructionOperand(const contract::Instruction& instruction, const VRegOperandFn& visit);
void forEachTerminatorOperand(const contract::Terminator& terminator, const VRegOperandFn& visit);

[[nodiscard]] bool instructionReferencesVReg(const contract::Instruction& instruction,
                                             std::string_view vreg);
[[nodiscard]] bool terminatorReferencesVReg(const contract::Terminator& terminator,
                                            std::string_view vreg);
[[nodiscard]] bool basicBlockReferencesVReg(const contract::BasicBlock& block, std::string_view vreg);

} // namespace toyc::codegen
