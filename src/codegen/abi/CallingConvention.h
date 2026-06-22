#pragma once

#include "codegen/ContractIR.h"
#include "codegen/frame/StackFrame.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace toyc::codegen {

class RiscvEmitter;

class CallingConvention {
public:
    static constexpr std::array<std::string_view, 8> kArgRegs = {
        "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    };

    CallingConvention(RiscvEmitter& emitter, const StackFrame& frame);

    [[nodiscard]] static int stackArgBytesFor(std::size_t argCount);

    void emitPrologue() const;
    void emitEpilogue() const;
    void emitParamLanding(const contract::IRFunction& function) const;
    void emitCallArgs(const std::vector<std::string>& args) const;
    void emitCallCleanup(std::size_t argCount) const;
    void loadReturnValue(std::string_view vreg) const;
    void loadVReg(std::string_view reg, std::string_view vreg) const;
    void storeVReg(std::string_view vreg, std::string_view reg) const;

private:
    RiscvEmitter& emitter_;
    const StackFrame& frame_;
};

} // namespace toyc::codegen
