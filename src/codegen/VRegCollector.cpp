#include "codegen/VRegCollector.h"

#include "codegen/CallingConvention.h"

#include <algorithm>
#include <type_traits>
#include <variant>

namespace toyc::codegen {

namespace {

template <typename Inst>
void collectBinaryVRegs(const Inst& inst, StackFrame& frame) {
    frame.addVReg(inst.dst);
    frame.addVReg(inst.src1);
    frame.addVReg(inst.src2);
}

template <typename Inst>
void collectUnaryVRegs(const Inst& inst, StackFrame& frame) {
    frame.addVReg(inst.dst);
    frame.addVReg(inst.src);
}

void collectCallArgs(const std::vector<std::string>& args, StackFrame& frame) {
    for (const std::string& arg : args) {
        frame.addVReg(arg);
    }
}

} // namespace

void VRegCollector::collectInto(const contract::IRFunction& function, StackFrame& frame) {
    int maxOutgoingArgBytes = 0;

    for (const contract::Param& param : function.params) {
        frame.addVReg(param.vreg);
    }

    for (const contract::BasicBlock& block : function.basicBlocks) {
        for (const contract::Instruction& instruction : block.instructions) {
            std::visit(
                [&](const auto& inst) {
                    using Inst = std::decay_t<decltype(inst)>;
                    if constexpr (std::is_same_v<Inst, contract::ConstInst>) {
                        frame.addVReg(inst.dst);
                    } else if constexpr (std::is_same_v<Inst, contract::CopyInst>) {
                        frame.addVReg(inst.dst);
                        frame.addVReg(inst.src);
                    } else if constexpr (std::is_same_v<Inst, contract::LoadGlobalInst>) {
                        frame.addVReg(inst.dst);
                    } else if constexpr (std::is_same_v<Inst, contract::StoreGlobalInst>) {
                        frame.addVReg(inst.src);
                    } else if constexpr (std::is_same_v<Inst, contract::CallInst>) {
                        frame.addVReg(inst.dst);
                        collectCallArgs(inst.args, frame);
                        maxOutgoingArgBytes = std::max(
                            maxOutgoingArgBytes,
                            CallingConvention::stackArgBytesFor(inst.args.size()));
                    } else if constexpr (std::is_same_v<Inst, contract::CallVoidInst>) {
                        collectCallArgs(inst.args, frame);
                        maxOutgoingArgBytes = std::max(
                            maxOutgoingArgBytes,
                            CallingConvention::stackArgBytesFor(inst.args.size()));
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
                        collectBinaryVRegs(inst, frame);
                    } else if constexpr (std::is_same_v<Inst, contract::NegInst> ||
                                         std::is_same_v<Inst, contract::LNotInst>) {
                        collectUnaryVRegs(inst, frame);
                    }
                },
                instruction);
        }

        std::visit(
            [&](const auto& terminator) {
                using Terminator = std::decay_t<decltype(terminator)>;
                if constexpr (std::is_same_v<Terminator, contract::BranchInst>) {
                    frame.addVReg(terminator.cond);
                } else if constexpr (std::is_same_v<Terminator, contract::ReturnInst>) {
                    if (terminator.src.has_value()) {
                        frame.addVReg(*terminator.src);
                    }
                }
            },
            block.terminator);
    }

    frame.setOutgoingArgBytes(maxOutgoingArgBytes);
}

} // namespace toyc::codegen
