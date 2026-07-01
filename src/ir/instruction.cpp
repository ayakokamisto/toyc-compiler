#include "toyc/ir/instruction.h"

// ---------------------------------------------------------------------------
// BinaryOpInstr
// ---------------------------------------------------------------------------

const char* BinaryOpInstr::op_name(Op op) {
    switch (op) {
    case Op::Add: return "add";
    case Op::Sub: return "sub";
    case Op::Mul: return "mul";
    case Op::Div: return "div";
    case Op::Mod: return "mod";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// UnaryOpInstr
// ---------------------------------------------------------------------------

const char* UnaryOpInstr::op_name(Op op) {
    switch (op) {
    case Op::Neg: return "neg";
    case Op::Not: return "not";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// CompareInstr
// ---------------------------------------------------------------------------

const char* CompareInstr::pred_name(Predicate p) {
    switch (p) {
    case Predicate::Lt: return "lt";
    case Predicate::Gt: return "gt";
    case Predicate::Le: return "le";
    case Predicate::Ge: return "ge";
    case Predicate::Eq: return "eq";
    case Predicate::Ne: return "ne";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// CallInstr
// ---------------------------------------------------------------------------

std::vector<Value*> CallInstr::operands() const {
    // Return a copy of args_ — the Java semantics (List.copyOf defensive copy)
    // is unnecessary here since we own args_ and return a fresh vector.
    return args_;
}

// ---------------------------------------------------------------------------
// PhiInstr
// ---------------------------------------------------------------------------

void PhiInstr::add_incoming(Label* pred, Value* val) {
    incoming_.push_back({pred, val});
}

std::vector<Value*> PhiInstr::operands() const {
    std::vector<Value*> ops;
    ops.reserve(incoming_.size());
    for (const auto& inc : incoming_) {
        ops.push_back(inc.value);
    }
    return ops;
}

// ---------------------------------------------------------------------------
// ReturnInstr
// ---------------------------------------------------------------------------

std::vector<Value*> ReturnInstr::operands() const {
    if (value_) {
        return {value_};
    }
    return {};
}
