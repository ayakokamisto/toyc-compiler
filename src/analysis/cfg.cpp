/// CFG construction — rebuilds predecessor/successor edges from terminators.

#include "toyc/analysis/cfg.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"

namespace toyc {

void rebuildCFG(Function& func) {
  for (auto& bb : func.blocks()) {
    bb->clearEdges();
  }

  for (auto& bb : func.blocks()) {
    if (!bb->hasTerminator()) continue;

    const auto& term = *bb->terminator();
    switch (term.opcode) {
      case Opcode::Br: {
        BlockId target = term.branchTarget;
        bb->addSuccessor(target);
        for (auto& targetBB : func.blocks()) {
          if (targetBB->id() == target) {
            targetBB->addPredecessor(bb->id());
            break;
          }
        }
        break;
      }
      case Opcode::CondBr: {
        BlockId trueT = term.condTrueTarget;
        BlockId falseT = term.condFalseTarget;
        bb->addSuccessor(trueT);
        if (falseT != trueT) {
          bb->addSuccessor(falseT);
        }
        for (auto& targetBB : func.blocks()) {
          if (targetBB->id() == trueT) {
            targetBB->addPredecessor(bb->id());
          }
          if (targetBB->id() == falseT && falseT != trueT) {
            targetBB->addPredecessor(bb->id());
          }
        }
        break;
      }
      case Opcode::Ret:
        break;
      default:
        break;
    }
  }
}

void rebuildCFG(Module& module) {
  for (auto& func : module.functions()) {
    rebuildCFG(*func);
  }
}

} // namespace toyc
