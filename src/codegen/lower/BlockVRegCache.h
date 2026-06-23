#pragma once

#include "codegen/abi/CallingConvention.h"
#include "codegen/emit/RiscvEmitter.h"

#include <optional>
#include <string>
#include <string_view>

namespace toyc::codegen {

// Tracks which vreg value currently lives in t0/t1 within one basic block.
// Caller-saved temps are invalidated at block boundaries and after calls.
class BlockVRegCache {
public:
    void invalidateAll() {
        t0_.reset();
        t1_.reset();
    }

    void forgetVReg(std::string_view vreg) { dropVReg(vreg); }

    void clobberRegister(std::string_view reg) {
        if (reg == "t0") {
            t0_.reset();
        } else if (reg == "t1") {
            t1_.reset();
        }
    }

    void load(CallingConvention& abi, RiscvEmitter& emitter, std::string_view reg, std::string_view vreg) {
        if (const std::optional<std::string_view> holder = findHolder(vreg)) {
            if (*holder != reg) {
                emitter.instruction("mv", {reg, *holder});
            }
            setRegister(reg, vreg);
            return;
        }

        abi.loadVReg(reg, vreg);
        setRegister(reg, vreg);
    }

    void store(CallingConvention& abi, std::string_view vreg, std::string_view reg) {
        abi.storeVReg(vreg, reg);
        dropVReg(vreg);
        setRegister(reg, vreg);
    }

private:
    std::optional<std::string> t0_;
    std::optional<std::string> t1_;

    [[nodiscard]] std::optional<std::string_view> findHolder(std::string_view vreg) const {
        if (t0_ == vreg) {
            return "t0";
        }
        if (t1_ == vreg) {
            return "t1";
        }
        return std::nullopt;
    }

    void dropVReg(std::string_view vreg) {
        if (t0_ == vreg) {
            t0_.reset();
        }
        if (t1_ == vreg) {
            t1_.reset();
        }
    }

    void setRegister(std::string_view reg, std::string_view vreg) {
        clobberRegister(reg);
        const std::string bound(vreg);
        if (reg == "t0") {
            t0_ = bound;
        } else if (reg == "t1") {
            t1_ = bound;
        }
    }
};

} // namespace toyc::codegen
