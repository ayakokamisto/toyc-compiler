#include "codegen/IrOperandUtils.h"

#include <type_traits>
#include <variant>

namespace toyc::codegen {

namespace {

void visitCallArgs(const std::vector<std::string>& args, const VRegOperandFn& visit) {
    for (const std::string& arg : args) {
        visit(arg);
    }
}

template <typename Inst>
void visitBinaryOperands(const Inst& inst, const VRegOperandFn& visit) {
    visit(inst.dst);
    visit(inst.src1);
    visit(inst.src2);
}

template <typename Inst>
void visitUnaryOperands(const Inst& inst, const VRegOperandFn& visit) {
    visit(inst.dst);
    visit(inst.src);
}

} // namespace

void forEachInstructionOperand(const contract::Instruction& instruction,
                               const VRegOperandFn& visit) {
    std::visit(
        [&](const auto& inst) {
            using Inst = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Inst, contract::ConstInst>) {
                visit(inst.dst);
            } else if constexpr (std::is_same_v<Inst, contract::CopyInst>) {
                visit(inst.dst);
                visit(inst.src);
            } else if constexpr (std::is_same_v<Inst, contract::LoadGlobalInst>) {
                visit(inst.dst);
            } else if constexpr (std::is_same_v<Inst, contract::StoreGlobalInst>) {
                visit(inst.src);
            } else if constexpr (std::is_same_v<Inst, contract::CallInst>) {
                visit(inst.dst);
                visitCallArgs(inst.args, visit);
            } else if constexpr (std::is_same_v<Inst, contract::CallVoidInst>) {
                visitCallArgs(inst.args, visit);
            } else if constexpr (std::is_same_v<Inst, contract::AddInst> ||
                                 std::is_same_v<Inst, contract::SubInst> ||
                                 std::is_same_v<Inst, contract::ModInst> ||
                                 std::is_same_v<Inst, contract::MulInst> ||
                                 std::is_same_v<Inst, contract::DivInst> ||
                                 std::is_same_v<Inst, contract::EqInst> ||
                                 std::is_same_v<Inst, contract::NeInst> ||
                                 std::is_same_v<Inst, contract::LtInst> ||
                                 std::is_same_v<Inst, contract::LeInst> ||
                                 std::is_same_v<Inst, contract::GtInst> ||
                                 std::is_same_v<Inst, contract::GeInst>) {
                visitBinaryOperands(inst, visit);
            } else if constexpr (std::is_same_v<Inst, contract::NegInst> ||
                                 std::is_same_v<Inst, contract::LNotInst>) {
                visitUnaryOperands(inst, visit);
            }
        },
        instruction);
}

void forEachTerminatorOperand(const contract::Terminator& terminator, const VRegOperandFn& visit) {
    std::visit(
        [&](const auto& inst) {
            using Terminator = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Terminator, contract::BranchInst>) {
                visit(inst.cond);
            } else if constexpr (std::is_same_v<Terminator, contract::ReturnInst>) {
                if (inst.src.has_value()) {
                    visit(*inst.src);
                }
            }
        },
        terminator);
}

bool instructionReferencesVReg(const contract::Instruction& instruction, std::string_view vreg) {
    if (vreg.empty()) {
        return false;
    }
    bool found = false;
    forEachInstructionOperand(instruction, [&](const std::string_view operand) {
        if (operand == vreg) {
            found = true;
        }
    });
    return found;
}

bool terminatorReferencesVReg(const contract::Terminator& terminator, std::string_view vreg) {
    if (vreg.empty()) {
        return false;
    }
    bool found = false;
    forEachTerminatorOperand(terminator, [&](const std::string_view operand) {
        if (operand == vreg) {
            found = true;
        }
    });
    return found;
}

bool basicBlockReferencesVReg(const contract::BasicBlock& block, std::string_view vreg) {
    for (const contract::Instruction& instruction : block.instructions) {
        if (instructionReferencesVReg(instruction, vreg)) {
            return true;
        }
    }
    return terminatorReferencesVReg(block.terminator, vreg);
}

} // namespace toyc::codegen
