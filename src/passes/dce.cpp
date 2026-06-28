#include "toyc/passes/dce.h"

#include "toyc/passes/ir_utils.h"

#include <algorithm>
#include <vector>

namespace toyc {

PassResult DCEPass::run(Function& function) {
  if (function.form() != IRForm::SSA) return {};

  bool changed = false;
  bool localChanged = true;
  while (localChanged) {
    localChanged = false;
    auto used = collectUsedValues(function);
    std::vector<ValueId> erased;

    for (auto& block : function.blocks()) {
      auto& insts = block->mutableInstructions();
      auto oldSize = insts.size();
      insts.erase(std::remove_if(insts.begin(), insts.end(), [&](const auto& inst) {
                    if (!inst->result.has_value()) return false;
                    if (used.contains(*inst->result)) return false;
                    if (!instructionIsRemovable(*inst)) return false;
                    erased.push_back(*inst->result);
                    return true;
                  }),
                  insts.end());
      if (insts.size() != oldSize) {
        localChanged = true;
        changed = true;
      }
    }
    function.eraseValues(erased);
  }

  return {changed};
}

} // namespace toyc
