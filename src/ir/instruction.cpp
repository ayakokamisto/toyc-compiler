#include "toyc/ir/instruction.h"

// ---------------------------------------------------------------------------
// LoadInstr
// ---------------------------------------------------------------------------

void LoadInstr::replace_operand(Value* old_value, Value* replacement) {
    if (address_ == old_value) address_ = replacement;
}

// ---------------------------------------------------------------------------
// StoreInstr
// ---------------------------------------------------------------------------

void StoreInstr::replace_operand(Value* old_value, Value* replacement) {
    if (value_ == old_value) value_ = replacement;
    if (address_ == old_value) address_ = replacement;
}

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

void BinaryOpInstr::replace_operand(Value* old_value, Value* replacement) {
    if (left_ == old_value) left_ = replacement;
    if (right_ == old_value) right_ = replacement;
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

void UnaryOpInstr::replace_operand(Value* old_value, Value* replacement) {
    if (value_ == old_value) value_ = replacement;
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

void CompareInstr::replace_operand(Value* old_value, Value* replacement) {
    if (left_ == old_value) left_ = replacement;
    if (right_ == old_value) right_ = replacement;
}

// ---------------------------------------------------------------------------
// CallInstr
// ---------------------------------------------------------------------------

std::vector<Value*> CallInstr::operands() const {
    // Return a copy of args_ — the Java semantics (List.copyOf defensive copy)
    // is unnecessary here since we own args_ and return a fresh vector.
    return args_;
}

void CallInstr::replace_operand(Value* old_value, Value* replacement) {
    for (Value*& arg : args_) {
        if (arg == old_value) arg = replacement;
    }
}

// ---------------------------------------------------------------------------
// MoveInstr
// ---------------------------------------------------------------------------

void MoveInstr::replace_operand(Value* old_value, Value* replacement) {
    if (value_ == old_value) value_ = replacement;
}

// ---------------------------------------------------------------------------
// PhiInstr
// ---------------------------------------------------------------------------

void PhiInstr::add_incoming(Label* pred, Value* val) {
    incoming_.push_back({pred, val});
}

void PhiInstr::replaceIncomingValue(Label* predecessor, Value* oldValue, Value* newValue) {
    for (auto& inc : incoming_) {
        if (inc.predecessor == predecessor && inc.value == oldValue) {
            inc.value = newValue;
        }
    }
}

void PhiInstr::replace_operand(Value* old_value, Value* replacement) {
    for (auto& inc : incoming_) {
        if (inc.value == old_value) inc.value = replacement;
    }
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
// CondBranchInstr
// ---------------------------------------------------------------------------

void CondBranchInstr::replace_operand(Value* old_value, Value* replacement) {
    if (condition_ == old_value) condition_ = replacement;
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

void ReturnInstr::replace_operand(Value* old_value, Value* replacement) {
    if (value_ == old_value) value_ = replacement;
}
