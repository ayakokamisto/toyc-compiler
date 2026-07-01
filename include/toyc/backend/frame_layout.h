#pragma once
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include <cstdint>
#include <optional>
#include <unordered_map>

struct FrameLayout {
    std::unordered_map<const Value*, uint32_t> valueHome;
    std::unordered_map<const Value*, uint32_t> allocaHome;
    uint32_t raOffset = 0;
    uint32_t s0Offset = 0;
    uint32_t outgoingArgBytes = 0;   // static area for this function's call args 9+
    uint32_t frameSize = 0;

    // Offset from s0 for outgoing arg #argIndex (0-based, 0 = arg 9)
    uint32_t outgoingArgOffset(uint32_t argIndex) const {
        return argIndex * 4;
    }

    // Offset from s0 for this function's incoming stack param #paramIndex (9+)
    uint32_t incomingStackArgOffset(uint32_t paramIndex) const {
        return frameSize + (paramIndex - 8) * 4;
    }

    bool isAllocaAddress(const Value* value) const;
    bool hasValueHome(const Value* value) const;
    std::optional<uint32_t> allocaOffset(const Value* value) const;
    std::optional<uint32_t> valueOffset(const Value* value) const;

    static FrameLayout compute(const Function& fn);
};

constexpr uint32_t align_up(uint32_t val, uint32_t alignment) {
    return (val + alignment - 1) & ~(alignment - 1);
}
