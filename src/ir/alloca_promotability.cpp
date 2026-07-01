#include "toyc/ir/alloca_promotability.h"

#include <algorithm>

AllocaPromotabilityAnalysis::AllocaPromotabilityAnalysis(
    const Function& function,
    const DefUseIndex& defUse)
    : function_(function), defUse_(defUse) {
    const BasicBlock* entry = function_.entry_block();
    if (entry != nullptr) {
        for (const auto& instruction : entry->instructions()) {
            if (instruction->kind() != InstrKind::Alloca) break;
            entry_alloca_prefix_.push_back(instruction->result());
        }
    }

    for (const auto& block : function_.blocks()) {
        for (const Instruction* instruction : block->all_instrs()) {
            if (instruction->kind() == InstrKind::Alloca && instruction->result() != nullptr) {
                alloca_addresses_.push_back(instruction->result());
                infos_.emplace(instruction->result(), classify(*instruction->result()));
            }
        }
    }
}

const AllocaPromotionInfo& AllocaPromotabilityAnalysis::analyze(
    const Value& allocaAddress) const {
    static const AllocaPromotionInfo not_alloca{
        nullptr,
        AllocaPromotionKind::UnsupportedUse,
        {},
        "value is not an alloca address",
    };
    auto it = infos_.find(&allocaAddress);
    return it == infos_.end() ? not_alloca : it->second;
}

std::vector<const Value*> AllocaPromotabilityAnalysis::promotableAllocas() const {
    std::vector<const Value*> out;
    for (const Value* value : entry_alloca_prefix_) {
        auto it = infos_.find(value);
        if (it != infos_.end() && it->second.kind == AllocaPromotionKind::Promotable) {
            out.push_back(value);
        }
    }
    return out;
}

AllocaPromotionInfo AllocaPromotabilityAnalysis::classify(const Value& value) const {
    const Instruction* def = defUse_.definitionOf(value);
    AllocaPromotionInfo info{&value, AllocaPromotionKind::Promotable, {}, "promotable"};
    if (def == nullptr || def->kind() != InstrKind::Alloca) {
        info.kind = AllocaPromotionKind::UnsupportedUse;
        info.reason = "value is not defined by alloca";
        return info;
    }
    if (!isEntryPrefixAlloca(*def)) {
        info.kind = AllocaPromotionKind::NotEntryAlloca;
        info.reason = "alloca is outside entry alloca prefix";
        return info;
    }

    for (const UseSite& use : defUse_.usesOf(value)) {
        const Instruction* user = use.user;
        if (auto* load = dynamic_cast<const LoadInstr*>(user)) {
            if (use.operandIndex == 0 && load->address() == &value) continue;
            info.kind = AllocaPromotionKind::InvalidAddressUse;
            info.blockingUses.push_back(use);
            info.reason = "load address operand does not match alloca";
            return info;
        }
        if (auto* store = dynamic_cast<const StoreInstr*>(user)) {
            if (use.operandIndex == 1 && store->address() == &value) continue;
            info.kind = AllocaPromotionKind::AddressEscapes;
            info.blockingUses.push_back(use);
            info.reason = "address escapes via store value";
            return info;
        }
        if (user->kind() == InstrKind::Call) {
            info.kind = AllocaPromotionKind::AddressEscapes;
            info.blockingUses.push_back(use);
            info.reason = "address escapes via call argument " + std::to_string(use.operandIndex);
            return info;
        }
        if (user->kind() == InstrKind::Return) {
            info.kind = AllocaPromotionKind::AddressEscapes;
            info.blockingUses.push_back(use);
            info.reason = "address escapes via return value";
            return info;
        }
        info.kind = AllocaPromotionKind::NonLoadStoreUse;
        info.blockingUses.push_back(use);
        info.reason = "address used by non-load/store instruction";
        return info;
    }
    return info;
}

bool AllocaPromotabilityAnalysis::isEntryPrefixAlloca(const Instruction& instruction) const {
    return std::find(entry_alloca_prefix_.begin(), entry_alloca_prefix_.end(), instruction.result())
        != entry_alloca_prefix_.end();
}

const char* to_string(AllocaPromotionKind kind) {
    switch (kind) {
    case AllocaPromotionKind::Promotable: return "promotable";
    case AllocaPromotionKind::NotEntryAlloca: return "non-promotable";
    case AllocaPromotionKind::AddressEscapes: return "non-promotable";
    case AllocaPromotionKind::UnsupportedUse: return "non-promotable";
    case AllocaPromotionKind::NonLoadStoreUse: return "non-promotable";
    case AllocaPromotionKind::InvalidAddressUse: return "non-promotable";
    }
    return "non-promotable";
}
