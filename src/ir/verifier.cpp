/// IR Verifier — checks structural well-formedness of the IR module.

#include "toyc/ir/verifier.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"

#include <set>

namespace toyc {

static bool isValueDefined(const Function& func, ValueId vid) {
  for (const auto& v : func.values()) {
    if (v.id == vid) return true;
  }
  return false;
}

static bool slotExists(const Function& func, SlotId sid) {
  for (const auto& s : func.slots()) {
    if (s.id == sid) return true;
  }
  return false;
}

static bool blockExists(const Function& func, BlockId bid) {
  for (const auto& bb : func.blocks()) {
    if (bb->id() == bid) return true;
  }
  return false;
}

VerificationResult verifyModule(const Module& module) {
  VerificationResult result;

  std::set<uint32_t> globalIds;
  for (const auto& g : module.globals()) {
    if (!globalIds.insert(g.id.value).second) {
      result.addError("Duplicate GlobalId: " + std::to_string(g.id.value));
    }
  }

  std::set<uint32_t> funcIds;
  for (const auto& f : module.functions()) {
    if (!funcIds.insert(f->id().value).second) {
      result.addError("Duplicate FunctionId: " + std::to_string(f->id().value));
    }
  }

  std::set<std::string> globalNames;
  for (const auto& g : module.globals()) {
    if (!globalNames.insert(g.name).second) {
      result.addError("Duplicate global name: " + g.name);
    }
  }

  std::set<std::string> funcNames;
  for (const auto& f : module.functions()) {
    if (!funcNames.insert(f->name()).second) {
      result.addError("Duplicate function name: " + f->name());
    }
  }

  for (const auto& func : module.functions()) {
    auto funcResult = verifyFunction(*func, module);
    for (auto& err : funcResult.errors) {
      result.addError(std::move(err));
    }
  }

  return result;
}

static void verifyInst(VerificationResult& result, const Function& func, const Module& module,
                       const Inst& inst, const BasicBlock& block) {
  auto checkValue = [&](ValueId vid, const char* ctx) {
    if (!isValueDefined(func, vid)) {
      result.addError("Undefined value " + std::to_string(vid.value) + " in " + ctx +
                      " of block " + block.label());
    }
  };

  auto checkSlot = [&](SlotId sid, const char* ctx) {
    if (!slotExists(func, sid)) {
      result.addError("Invalid SlotId " + std::to_string(sid.value) + " in " + ctx +
                      " of block " + block.label());
    }
  };

  switch (inst.opcode) {
    case Opcode::ConstInt:
      break;
    case Opcode::SlotLoad:
      checkSlot(inst.slot, "load.slot");
      break;
    case Opcode::SlotStore:
      checkSlot(inst.slot, "store.slot");
      checkValue(inst.lhs, "store.slot value");
      break;
    case Opcode::GlobalLoad: {
      const auto* g = module.findGlobal(inst.global);
      if (!g) {
        result.addError("Invalid GlobalId in load.global of block " + block.label());
      }
      break;
    }
    case Opcode::GlobalStore: {
      const auto* g = module.findGlobal(inst.global);
      if (!g) {
        result.addError("Invalid GlobalId in store.global of block " + block.label());
      }
      checkValue(inst.lhs, "store.global value");
      break;
    }
    case Opcode::Unary:
      checkValue(inst.unaryOperand, "unary operand");
      break;
    case Opcode::Binary:
      checkValue(inst.lhs, "binary lhs");
      checkValue(inst.rhs, "binary rhs");
      break;
    case Opcode::Compare:
      checkValue(inst.cmpLhs, "compare lhs");
      checkValue(inst.cmpRhs, "compare rhs");
      break;
    case Opcode::Call: {
      const auto* callee = module.findFunction(inst.callee);
      if (!callee) {
        result.addError("Invalid FunctionId in call of block " + block.label());
        break;
      }
      if (inst.arguments.size() != callee->params().size()) {
        result.addError("Call argument count mismatch: expected " +
                        std::to_string(callee->params().size()) + " got " +
                        std::to_string(inst.arguments.size()) +
                        " in call to " + callee->name());
      }
      for (const auto& arg : inst.arguments) {
        checkValue(arg, "call argument");
      }
      bool calleeReturnsInt = callee->returnType().isI32();
      if (calleeReturnsInt && !inst.result.has_value()) {
        result.addError("Call to int function " + callee->name() + " missing result in block " + block.label());
      }
      if (!calleeReturnsInt && inst.result.has_value()) {
        result.addError("Call to void function " + callee->name() + " has result in block " + block.label());
      }
      break;
    }
    case Opcode::Phi:
      for (const auto& inc : inst.phiIncoming) {
        checkValue(inc.value, "phi incoming value");
        if (!blockExists(func, inc.block)) {
          result.addError("Invalid BlockId in phi incoming of block " + block.label());
        }
      }
      break;
    default:
      break;
  }
}

static void verifyTerminator(VerificationResult& result, const Function& func,
                             const Terminator& term, const BasicBlock& block) {
  switch (term.opcode) {
    case Opcode::Br:
      if (!blockExists(func, term.branchTarget)) {
        result.addError("Invalid branch target in block " + block.label());
      }
      break;
    case Opcode::CondBr:
      if (!isValueDefined(func, term.condCondition)) {
        result.addError("Undefined condition value in condbr of block " + block.label());
      }
      if (!blockExists(func, term.condTrueTarget)) {
        result.addError("Invalid true target in condbr of block " + block.label());
      }
      if (!blockExists(func, term.condFalseTarget)) {
        result.addError("Invalid false target in condbr of block " + block.label());
      }
      break;
    case Opcode::Ret:
      if (func.returnType().isI32()) {
        if (!term.returnValue.has_value()) {
          result.addError("Int function " + func.name() + " missing return value in block " + block.label());
        } else if (!isValueDefined(func, *term.returnValue)) {
          result.addError("Undefined return value in block " + block.label());
        }
      } else if (func.returnType().isVoid()) {
        if (term.returnValue.has_value()) {
          result.addError("Void function " + func.name() + " has return value in block " + block.label());
        }
      }
      break;
    default:
      result.addError("Unknown terminator opcode in block " + block.label());
      break;
  }
}

VerificationResult verifyFunction(const Function& func, const Module& module) {
  VerificationResult result;

  if (func.blocks().empty()) {
    result.addError("Function " + func.name() + " has no blocks");
    return result;
  }

  std::set<std::string> labels;
  for (const auto& bb : func.blocks()) {
    if (!labels.insert(bb->label()).second) {
      result.addError("Duplicate block label: " + bb->label() + " in function " + func.name());
    }
  }

  for (const auto& bb : func.blocks()) {
    if (!bb->hasTerminator()) {
      result.addError("Block " + bb->label() + " in function " + func.name() + " has no terminator");
    }

    for (const auto& inst : bb->instructions()) {
      verifyInst(result, func, module, *inst, *bb);
    }

    if (bb->hasTerminator()) {
      verifyTerminator(result, func, *bb->terminator(), *bb);
    }
  }

  std::set<uint32_t> valueIds;
  for (const auto& v : func.values()) {
    if (!valueIds.insert(v.id.value).second) {
      result.addError("Duplicate ValueId " + std::to_string(v.id.value) + " in function " + func.name());
    }
  }

  return result;
}

} // namespace toyc
