#include "codegen/abi/CallingConvention.h"

#include "codegen/emit/CodegenUtils.h"
#include "codegen/emit/RiscvEmitter.h"

namespace toyc::codegen {

namespace {

constexpr int kWordBytes = 4;

} // namespace

CallingConvention::CallingConvention(RiscvEmitter& emitter,
                                     const StackFrame& frame,
                                     const VRegAssignment& assignment)
    : emitter_(emitter), frame_(frame), assignment_(assignment) {}

int CallingConvention::stackArgBytesFor(std::size_t argCount) {
    if (argCount <= kArgRegs.size()) {
        return 0;
    }
    return alignTo16(static_cast<int>(argCount - kArgRegs.size()) * kWordBytes);
}

void CallingConvention::emitPrologue() const {
    if (frame_.frameSizeBytes() > 0) {
        emitter_.instruction("addi", {"sp", "sp", imm(-frame_.frameSizeBytes())});
    }
    for (const SavedRegisterSlot& slot : frame_.savedRegisterSlots()) {
        emitter_.instruction("sw", {slot.reg, offsetReg(slot.offsetFromSp, "sp")});
    }
    if (frame_.usesFramePointer()) {
        emitter_.instruction("addi", {"s0", "sp", imm(frame_.frameSizeBytes())});
    }
}

void CallingConvention::emitEpilogue() const {
    for (const SavedRegisterSlot& slot : frame_.savedRegisterSlots()) {
        emitter_.instruction("lw", {slot.reg, offsetReg(slot.offsetFromSp, "sp")});
    }
    if (frame_.frameSizeBytes() > 0) {
        emitter_.instruction("addi", {"sp", "sp", imm(frame_.frameSizeBytes())});
    }
    emitter_.instruction("ret");
}

void CallingConvention::emitParamLanding(const contract::IRFunction& function) const {
    for (std::size_t i = 0; i < function.params.size(); ++i) {
        const contract::Param& param = function.params[i];
        if (const std::optional<std::string_view> physReg = assignment_.physicalReg(param.vreg)) {
            if (i < kArgRegs.size()) {
                emitter_.instruction("mv", {*physReg, kArgRegs[i]});
            } else {
                const int incomingStackOffset =
                    static_cast<int>((i - kArgRegs.size()) * kWordBytes);
                emitter_.instruction("lw", {"t0", offsetReg(incomingStackOffset, "s0")});
                emitter_.instruction("mv", {*physReg, "t0"});
            }
            continue;
        }

        const int slotOffset = frame_.vregOffsetFromS0(param.vreg);
        if (i < kArgRegs.size()) {
            emitter_.instruction("sw", {kArgRegs[i], offsetReg(slotOffset, "s0")});
            continue;
        }

        const int incomingStackOffset = static_cast<int>((i - kArgRegs.size()) * kWordBytes);
        emitter_.instruction("lw", {"t0", offsetReg(incomingStackOffset, "s0")});
        emitter_.instruction("sw", {"t0", offsetReg(slotOffset, "s0")});
    }
}

void CallingConvention::loadVReg(std::string_view reg, std::string_view vreg) const {
    if (const std::optional<std::string_view> physReg = assignment_.physicalReg(vreg)) {
        if (*physReg != reg) {
            emitter_.instruction("mv", {reg, *physReg});
        }
        return;
    }
    if (const std::optional<std::int32_t> value = assignment_.rematerializedConstant(vreg)) {
        if (*value >= -2048 && *value <= 2047) {
            emitter_.instruction("addi", {reg, "zero", imm(*value)});
        } else {
            emitter_.instruction("li", {reg, imm(*value)});
        }
        return;
    }
    emitter_.instruction("lw", {reg, offsetReg(frame_.vregOffsetFromS0(vreg), "s0")});
}

void CallingConvention::storeVReg(std::string_view vreg, std::string_view reg) const {
    if (const std::optional<std::string_view> physReg = assignment_.physicalReg(vreg)) {
        if (*physReg != reg) {
            emitter_.instruction("mv", {*physReg, reg});
        }
        return;
    }
    if (assignment_.rematerializedConstant(vreg).has_value()) {
        return;
    }
    emitter_.instruction("sw", {reg, offsetReg(frame_.vregOffsetFromS0(vreg), "s0")});
}

void CallingConvention::loadReturnValue(std::string_view vreg) const {
    loadVReg("a0", vreg);
}

void CallingConvention::emitCallArgs(const std::vector<std::string>& args) const {
    const int stackArgBytes = stackArgBytesFor(args.size());
    if (stackArgBytes > 0) {
        emitter_.instruction("addi", {"sp", "sp", imm(-stackArgBytes)});
    }

    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i < kArgRegs.size()) {
            loadVReg(kArgRegs[i], args[i]);
            continue;
        }

        loadVReg("t0", args[i]);
        const int stackOffset = static_cast<int>((i - kArgRegs.size()) * kWordBytes);
        emitter_.instruction("sw", {"t0", offsetReg(stackOffset, "sp")});
    }
}

void CallingConvention::emitCallCleanup(std::size_t argCount) const {
    const int stackArgBytes = stackArgBytesFor(argCount);
    if (stackArgBytes > 0) {
        emitter_.instruction("addi", {"sp", "sp", imm(stackArgBytes)});
    }
}

} // namespace toyc::codegen
