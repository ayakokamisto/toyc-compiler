/// IR Printer — outputs IR in a stable, deterministic human-readable format.

#include "toyc/ir/printer.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"

#include <sstream>

namespace toyc {

static std::string valueStr(ValueId v) {
  return "%v" + std::to_string(v.value);
}

static std::string slotStr(SlotId s) {
  return "$slot" + std::to_string(s.value);
}

static std::string blockLabel(const Function& func, BlockId bid) {
  for (const auto& bb : func.blocks()) {
    if (bb->id() == bid) return bb->label();
  }
  return "block" + std::to_string(bid.value);
}

static std::string globalName(const Module& module, GlobalId gid) {
  const auto* g = module.findGlobal(gid);
  return g ? ("@" + g->name) : ("@global" + std::to_string(gid.value));
}

static std::string funcName(const Module& module, FunctionId fid) {
  const auto* f = module.findFunction(fid);
  return f ? ("@" + f->name()) : ("@func" + std::to_string(fid.value));
}

static void dumpGlobal(std::ostream& out, const IRGlobal& g) {
  out << "    @" << g.name;
  out << " kind=";
  switch (g.kind) {
    case GlobalKind::Variable:      out << "variable"; break;
    case GlobalKind::Constant:      out << "constant"; break;
    case GlobalKind::InternalGuard: out << "internal-guard"; break;
  }
  out << " init=";
  switch (g.initKind) {
    case IRGlobalInitKind::Static:                out << "static"; break;
    case IRGlobalInitKind::RuntimeZeroInitialized: out << "runtime-zero"; break;
  }
  out << " value=" << g.staticInitialValue << "\n";
}

static void dumpInst(std::ostream& out, const Module& module, const Inst& inst,
                     const std::string& indent) {
  if (inst.result.has_value()) {
    out << indent << valueStr(*inst.result) << " = ";
  } else {
    out << indent;
  }

  switch (inst.opcode) {
    case Opcode::ConstInt:
      out << "const " << inst.constValue;
      break;
    case Opcode::SlotLoad:
      out << "load.slot " << slotStr(inst.slot);
      break;
    case Opcode::SlotStore:
      out << "store.slot " << slotStr(inst.slot) << ", " << valueStr(inst.lhs);
      break;
    case Opcode::GlobalLoad:
      out << "load.global " << globalName(module, inst.global);
      break;
    case Opcode::GlobalStore:
      out << "store.global " << globalName(module, inst.global) << ", " << valueStr(inst.lhs);
      break;
    case Opcode::Unary:
      out << unaryOpcodeName(inst.unaryOp) << " " << valueStr(inst.unaryOperand);
      break;
    case Opcode::Binary:
      out << binaryOpcodeName(inst.binaryOp) << " "
          << valueStr(inst.lhs) << ", " << valueStr(inst.rhs);
      break;
    case Opcode::Compare:
      out << "cmp." << comparePredicateName(inst.cmpPred) << " "
          << valueStr(inst.cmpLhs) << ", " << valueStr(inst.cmpRhs);
      break;
    case Opcode::Call: {
      out << "call " << funcName(module, inst.callee) << "(";
      for (size_t i = 0; i < inst.arguments.size(); ++i) {
        if (i > 0) out << ", ";
        out << valueStr(inst.arguments[i]);
      }
      out << ")";
      break;
    }
    case Opcode::Phi:
      out << "phi";
      for (const auto& inc : inst.phiIncoming) {
        out << " [" << valueStr(inc.value) << ", " << blockLabel(*module.findFunction(FunctionId{0}), inc.block) << "]";
      }
      break;
    default:
      out << "<unknown>";
      break;
  }
  out << "\n";
}

static void dumpTerminator(std::ostream& out, const Module& module, const Terminator& term,
                           const Function& func, const std::string& indent) {
  out << indent;
  switch (term.opcode) {
    case Opcode::Br:
      out << "br " << blockLabel(func, term.branchTarget);
      break;
    case Opcode::CondBr:
      out << "condbr " << valueStr(term.condCondition)
          << ", " << blockLabel(func, term.condTrueTarget)
          << ", " << blockLabel(func, term.condFalseTarget);
      break;
    case Opcode::Ret:
      if (term.returnValue.has_value()) {
        out << "ret " << valueStr(*term.returnValue);
      } else {
        out << "ret";
      }
      break;
    default:
      out << "<unknown terminator>";
      break;
  }
  out << "\n";
}

static void dumpSlot(std::ostream& out, const Slot& slot) {
  out << "      $" << slot.id.value << " kind=";
  switch (slot.kind) {
    case SlotKind::Parameter:     out << "parameter"; break;
    case SlotKind::LocalVariable: out << "local"; break;
    case SlotKind::Temporary:     out << "temporary"; break;
  }
  if (slot.sourceSymbol.has_value()) {
    out << " symbol=" << slot.sourceSymbol->value;
  }
  out << "\n";
}

static void dumpFunction(std::ostream& out, const Module& module, const Function& func) {
  out << "    func @" << func.name() << "(";
  for (size_t i = 0; i < func.params().size(); ++i) {
    if (i > 0) out << ", ";
    out << "i32 " << valueStr(func.params()[i].valueId);
  }
  out << ") -> " << func.returnType().toString() << "\n";

  if (!func.slots().empty()) {
    out << "      Slots\n";
    for (const auto& slot : func.slots()) {
      dumpSlot(out, slot);
    }
  }

  out << "      Blocks\n";
  for (const auto& bb : func.blocks()) {
    out << "        " << bb->label() << ":\n";
    for (const auto& inst : bb->instructions()) {
      dumpInst(out, module, *inst, "          ");
    }
    if (bb->hasTerminator()) {
      dumpTerminator(out, module, *bb->terminator(), func, "          ");
    }
  }
}

void dumpIR(const Module& module, std::ostream& output) {
  output << "IRModule\n";

  if (!module.globals().empty()) {
    output << "  Globals\n";
    for (const auto& g : module.globals()) {
      dumpGlobal(output, g);
    }
  }

  if (!module.functions().empty()) {
    output << "  Functions\n";
    for (const auto& func : module.functions()) {
      dumpFunction(output, module, *func);
    }
  }
}

} // namespace toyc
