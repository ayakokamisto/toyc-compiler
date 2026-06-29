#include "codegen/lower/BranchFusionAnalysis.h"

#include "codegen/IrOperandUtils.h"

#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

namespace toyc::codegen {

namespace {

const contract::BasicBlock* findBlockByLabel(const contract::IRFunction& function,
                                             std::string_view label) {
    for (const contract::BasicBlock& block : function.basicBlocks) {
        if (block.label == label) {
            return &block;
        }
    }
    return nullptr;
}

void appendSuccessors(const contract::Terminator& terminator, std::vector<std::string>& worklist) {
    if (const auto* jump = std::get_if<contract::JumpInst>(&terminator)) {
        worklist.push_back(jump->targetLabel);
        return;
    }
    if (const auto* branch = std::get_if<contract::BranchInst>(&terminator)) {
        worklist.push_back(branch->trueLabel);
        worklist.push_back(branch->falseLabel);
    }
}

} // namespace

bool isBranchCondUsedInTargets(const contract::IRFunction& function,
                               const contract::BranchInst& branch,
                               std::string_view branchBlockLabel) {
    std::vector<std::string> worklist;
    worklist.push_back(branch.trueLabel);
    worklist.push_back(branch.falseLabel);

    std::unordered_set<std::string> visited;
    while (!worklist.empty()) {
        const std::string label = worklist.back();
        worklist.pop_back();
        if (!visited.insert(label).second) {
            continue;
        }

        const contract::BasicBlock* block = findBlockByLabel(function, label);
        if (block == nullptr) {
            continue;
        }
        if (block->label != branchBlockLabel && basicBlockReferencesVReg(*block, branch.cond)) {
            return true;
        }
        appendSuccessors(block->terminator, worklist);
    }
    return false;
}

} // namespace toyc::codegen
