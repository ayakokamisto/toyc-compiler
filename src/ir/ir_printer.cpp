#include "toyc/ir/ir_printer.h"

#include <sstream>
#include <string>

// ===========================================================================
// Helpers
// ===========================================================================

static void indent(std::ostream& os, int depth) {
    for (int i = 0; i < depth; ++i) os << "  ";
}

template <typename Container, typename Accessor>
static void print_seq(std::ostream& os, const Container& c, Accessor&& acc,
                      const char* sep = ", ") {
    bool first = true;
    for (const auto& elem : c) {
        if (!first) os << sep;
        os << acc(elem);
        first = false;
    }
}

// ===========================================================================
// IRPrinter::print(Value)
// ===========================================================================

std::string IRPrinter::print(const Value& value) {
    return value.name();
}

// ===========================================================================
// IRPrinter::print(Instr)
// ===========================================================================

std::string IRPrinter::print(const Instr& instr) {
    std::ostringstream os;

    switch (instr.kind()) {
    case InstrKind::Alloca: {
        auto& a = static_cast<const AllocaInstr&>(instr);
        os << a.local()->name() << " = alloca";
        break;
    }
    case InstrKind::Load: {
        auto& l = static_cast<const LoadInstr&>(instr);
        os << l.temp()->name() << " = load " << l.address()->name();
        break;
    }
    case InstrKind::Store: {
        auto& s = static_cast<const StoreInstr&>(instr);
        os << "store " << s.value()->name() << ", " << s.address()->name();
        break;
    }
    case InstrKind::LoadImm: {
        auto& l = static_cast<const LoadImmInstr&>(instr);
        os << l.temp()->name() << " = imm " << l.int_value();
        break;
    }
    case InstrKind::BinaryOp: {
        auto& b = static_cast<const BinaryOpInstr&>(instr);
        os << b.temp()->name() << " = " << BinaryOpInstr::op_name(b.op())
           << " " << b.left()->name() << ", " << b.right()->name();
        break;
    }
    case InstrKind::UnaryOp: {
        auto& u = static_cast<const UnaryOpInstr&>(instr);
        os << u.temp()->name() << " = " << UnaryOpInstr::op_name(u.op())
           << " " << u.value()->name();
        break;
    }
    case InstrKind::Compare: {
        auto& c = static_cast<const CompareInstr&>(instr);
        os << c.temp()->name() << " = cmp " << CompareInstr::pred_name(c.predicate())
           << " " << c.left()->name() << ", " << c.right()->name();
        break;
    }
    case InstrKind::Call: {
        auto& c = static_cast<const CallInstr&>(instr);
        if (c.result()) {
            os << c.temp()->name() << " = ";
        }
        os << "call " << to_string(c.return_type()) << " " << c.callee() << "(";
        print_seq(os, c.args(), [](Value* v) { return v->name(); });
        os << ")";
        break;
    }
    case InstrKind::Move: {
        auto& m = static_cast<const MoveInstr&>(instr);
        os << m.temp()->name() << " = move " << m.value()->name();
        break;
    }
    case InstrKind::Phi: {
        auto& p = static_cast<const PhiInstr&>(instr);
        os << p.temp()->name() << " = phi";
        bool first = true;
        for (const auto& inc : p.incoming()) {
            if (!first) os << ",";
            os << " [" << inc.value->name() << ", " << inc.predecessor->name() << "]";
            first = false;
        }
        break;
    }
    case InstrKind::Branch: {
        auto& b = static_cast<const BranchInstr&>(instr);
        os << "br " << b.target()->name();
        break;
    }
    case InstrKind::CondBranch: {
        auto& c = static_cast<const CondBranchInstr&>(instr);
        os << "cbr " << c.condition()->name() << ", "
           << c.true_target()->name() << ", " << c.false_target()->name();
        break;
    }
    case InstrKind::Return: {
        auto& r = static_cast<const ReturnInstr&>(instr);
        if (r.has_value()) {
            os << "ret " << r.value()->name();
        } else {
            os << "ret";
        }
        break;
    }
    case InstrKind::GlobalAddr: {
        auto& g = static_cast<const GlobalAddrInstr&>(instr);
        os << g.temp()->name() << " = globaladdr " << g.global()->name();
        break;
    }
    }

    return os.str();
}

// ===========================================================================
// IRPrinter::print(BasicBlock)
// ===========================================================================

std::string IRPrinter::print(const BasicBlock& block) {
    std::ostringstream os;
    os << block.label()->name() << ":\n";

    for (auto* instr : block.all_instrs()) {
        os << "  " << IRPrinter::print(*instr) << "\n";
    }

    return os.str();
}

// ===========================================================================
// IRPrinter::print(Function)
// ===========================================================================

std::string IRPrinter::print(const Function& function) {
    std::ostringstream os;

    // Function header.
    os << "func @" << function.name() << "() -> " << to_string(function.return_type());
    os << " {\n";

    // Parameters / locals declaration.
    for (auto& [name, local] : function.locals()) {
        (void)name;
        if (!local->is_parameter()) {
            os << "  local " << local->name() << "\n";
        }
    }

    // Blocks in order.
    for (const auto& block : function.blocks()) {
        os << IRPrinter::print(*block);
    }

    os << "}\n";
    return os.str();
}

// ===========================================================================
// IRPrinter::print(Module)
// ===========================================================================

std::string IRPrinter::print(const Module& module) {
    std::ostringstream os;
    os << "module {\n";

    // Globals
    for (auto* gv : module.globals()) {
        os << "  global " << gv->name() << " = " << gv->initial_value() << "\n";
    }

    // Global constants
    for (auto* c : module.global_constants()) {
        os << "  const " << c->name() << " = " << c->value() << "\n";
    }

    os << "}\n";

    // Functions
    for (const auto& fn : module.functions()) {
        os << "\n" << IRPrinter::print(*fn);
    }

    return os.str();
}

// ===========================================================================
// IRPrinter::print(IRProgram)
// ===========================================================================

std::string IRPrinter::print(const IRProgram& program) {
    return IRPrinter::print(*program.module());
}
