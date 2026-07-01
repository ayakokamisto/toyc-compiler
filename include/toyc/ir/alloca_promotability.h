#pragma once

#include "def_use.h"

#include <string>
#include <unordered_map>

enum class AllocaPromotionKind {
    Promotable,
    NotEntryAlloca,
    AddressEscapes,
    UnsupportedUse,
    NonLoadStoreUse,
    InvalidAddressUse,
};

struct AllocaPromotionInfo {
    const Value* address;
    AllocaPromotionKind kind;
    std::vector<UseSite> blockingUses;
    std::string reason;
};

class AllocaPromotabilityAnalysis {
public:
    AllocaPromotabilityAnalysis(const Function& function, const DefUseIndex& defUse);

    const AllocaPromotionInfo& analyze(const Value& allocaAddress) const;
    std::vector<const Value*> promotableAllocas() const;
    const std::vector<const Value*>& entryAllocaPrefix() const { return entry_alloca_prefix_; }
    const std::vector<const Value*>& allocaAddresses() const { return alloca_addresses_; }

private:
    AllocaPromotionInfo classify(const Value& value) const;
    bool isEntryPrefixAlloca(const Instruction& instruction) const;

    const Function& function_;
    const DefUseIndex& defUse_;
    std::unordered_map<const Value*, AllocaPromotionInfo> infos_;
    std::vector<const Value*> entry_alloca_prefix_;
    std::vector<const Value*> alloca_addresses_;
};

const char* to_string(AllocaPromotionKind kind);
