#pragma once

#include "function.h"

#include <string>
#include <unordered_map>
#include <vector>

struct UseSite {
    const Instruction* user;
    std::size_t operandIndex;
};

class DefUseIndex {
public:
    explicit DefUseIndex(const Function& function);

    const Instruction* definitionOf(const Value& value) const;
    const std::vector<UseSite>& usesOf(const Value& value) const;
    bool hasSingleDefinition(const Value& value) const;

private:
    void addDefinition(const Instruction& instruction);
    void addUses(const Instruction& instruction);

    std::unordered_map<const Value*, const Instruction*> definitions_;
    std::unordered_map<const Value*, std::vector<UseSite>> uses_;
};

std::vector<std::string> verifyDefUseConsistency(
    const Function& function,
    const DefUseIndex& defUseIndex);

void replaceAllUsesWith(Function& function, Value& oldValue, Value& replacement);
