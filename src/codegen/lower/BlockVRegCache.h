#pragma once

#include "codegen/abi/CallingConvention.h"
#include "codegen/emit/RiscvEmitter.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace toyc::codegen {

// Tracks which vreg value currently lives in which physical register
// within one basic block.  Cached entries are invalidated at block
// boundaries, after calls, and when a register is explicitly clobbered.
//
// Tracks all allocated registers (t2–t6, s1–s11, plus scratch) so the
// selector can recognise when a value is already register-resident and
// avoid redundant loads.
class BlockVRegCache {
public:
    void invalidateAll() {
        vregToReg_.clear();
        regToVreg_.clear();
    }

    void forgetVReg(std::string_view vreg) { dropVReg(vreg); }

    void clobberRegister(std::string_view reg) {
        const auto it = regToVreg_.find(std::string(reg));
        if (it != regToVreg_.end()) {
            vregToReg_.erase(it->second);
            regToVreg_.erase(it);
        }
    }

    // Track a global address as a pseudo-vreg so the normal vreg cache
    // clobber mechanism automatically invalidates it when the register
    // is overwritten.
    void trackGlobalAddr(std::string_view reg, std::string_view globalName) {
        const std::string addrVReg = std::string(globalName) + "$addr";
        clobberRegister(reg);
        vregToReg_[addrVReg] = std::string(reg);
        regToVreg_[std::string(reg)] = addrVReg;
    }

    // Check whether a global address is still live in a register.
    [[nodiscard]] std::optional<std::string_view> findGlobalAddr(std::string_view globalName) const {
        const std::string addrVReg = std::string(globalName) + "$addr";
        return findHolder(addrVReg);
    }

    void load(CallingConvention& abi, RiscvEmitter& emitter,
              std::string_view reg, std::string_view vreg) {
        if (const std::optional<std::string_view> holder = findHolder(vreg)) {
            if (*holder != reg) {
                emitter.instruction("mv", {reg, *holder});
            }
            // Move tracking to the new register.
            regToVreg_.erase(std::string(*holder));
            regToVreg_[std::string(reg)] = std::string(vreg);
            vregToReg_[std::string(vreg)] = std::string(reg);
            return;
        }

        abi.loadVReg(reg, vreg);
        clobberRegister(reg);
        setRegister(reg, vreg);
    }

    void store(CallingConvention& abi, std::string_view vreg, std::string_view reg) {
        abi.storeVReg(vreg, reg);
        dropVReg(vreg);
        clobberRegister(reg);
        setRegister(reg, vreg);
    }

private:
    std::unordered_map<std::string, std::string> vregToReg_;
    std::unordered_map<std::string, std::string> regToVreg_;

    [[nodiscard]] std::optional<std::string_view> findHolder(std::string_view vreg) const {
        const auto it = vregToReg_.find(std::string(vreg));
        if (it == vregToReg_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void dropVReg(std::string_view vreg) {
        const auto it = vregToReg_.find(std::string(vreg));
        if (it != vregToReg_.end()) {
            regToVreg_.erase(it->second);
            vregToReg_.erase(it);
        }
    }

    void setRegister(std::string_view reg, std::string_view vreg) {
        // If this vreg was previously tracked in a different register,
        // clean up the stale regToVreg_ entry before it gets orphaned.
        // Otherwise a later clobberRegister on the old register would
        // incorrectly erase the vreg from vregToReg_ even though it
        // now lives in the new register.
        const auto oldIt = vregToReg_.find(std::string(vreg));
        if (oldIt != vregToReg_.end() && oldIt->second != reg) {
            regToVreg_.erase(oldIt->second);
        }
        clobberRegister(reg);
        const std::string boundVReg(vreg);
        const std::string boundReg(reg);
        vregToReg_[boundVReg] = boundReg;
        regToVreg_[boundReg] = boundVReg;
    }
};

} // namespace toyc::codegen
