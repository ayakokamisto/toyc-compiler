#include "toyc/backend/frame_layout.h"
#include <algorithm>

bool FrameLayout::isAllocaAddress(const Value* value) const {
    return allocaHome.find(value) != allocaHome.end();
}

bool FrameLayout::hasValueHome(const Value* value) const {
    return valueHome.find(value) != valueHome.end();
}

std::optional<uint32_t> FrameLayout::allocaOffset(const Value* value) const {
    auto it = allocaHome.find(value);
    if (it == allocaHome.end()) return std::nullopt;
    return it->second;
}

std::optional<uint32_t> FrameLayout::valueOffset(const Value* value) const {
    auto it = valueHome.find(value);
    if (it == valueHome.end()) return std::nullopt;
    return it->second;
}

FrameLayout FrameLayout::compute(const Function& fn) {
    FrameLayout layout;
    uint32_t maxCallArgs = 0;
    for (const auto& bb : fn.blocks()) {
        for (auto* instr : bb->all_instrs()) {
            if (instr->kind() == InstrKind::Call) {
                auto& call = static_cast<const CallInstr&>(*instr);
                maxCallArgs = std::max(maxCallArgs, (uint32_t)call.args().size());
            }
        }
    }
    layout.outgoingArgBytes = maxCallArgs > 8 ? (maxCallArgs - 8) * 4 : 0;

    uint32_t offset = layout.outgoingArgBytes;

    // Pass 1: valueHome for all non-alloca results after the outgoing area.
    for (const auto& bb : fn.blocks()) {
        for (auto* instr : bb->all_instrs()) {
            Value* result = instr->result();
            if (!result || instr->kind() == InstrKind::Alloca) continue;
            if (layout.valueHome.count(result)) continue;
            layout.valueHome[result] = offset;
            offset += 4;
        }
    }
    uint32_t valueHomeBytes = offset;

    // Pass 2: allocaHome for all Alloca address values after valueHome.
    for (const auto& bb : fn.blocks()) {
        for (auto* instr : bb->all_instrs()) {
            if (instr->kind() != InstrKind::Alloca) continue;
            Value* addr = instr->result();
            if (layout.allocaHome.count(addr)) continue;
            layout.allocaHome[addr] = offset;
            offset += 4;
        }
    }
    uint32_t allocaHomeBytes = offset - valueHomeBytes;
    (void)allocaHomeBytes;

    // Save area: ra (4 bytes) + s0 (4 bytes)
    uint32_t saveBytes = 8;
    uint32_t total = offset + saveBytes;
    layout.frameSize = align_up(total, 16);

    // Compute offsets relative to s0
    layout.raOffset = layout.frameSize - 4;
    layout.s0Offset = layout.frameSize - 8;

    if (layout.frameSize > 2047) {
        throw std::runtime_error("frame size " + std::to_string(layout.frameSize) +
            " exceeds P2-B3 limit of 2047 bytes");
    }
    return layout;
}
