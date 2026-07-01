#pragma once

#include "value.h"

#include <cstdint>
#include <string>
#include <vector>

// =============================================================================
// Instruction type hierarchy.
//
// Mirrors Java toyc.ir.inst.*. Each concrete class stores its own data fields
// and implements the virtual interface from Instr.
//
// Memory: instructions are owned by std::unique_ptr<Instr> in BasicBlock.
// Operands are non-owning Value* pointers into the Function's arena.
// =============================================================================

// ---------------------------------------------------------------------------
// InstrKind — runtime type tag for switch/dispatch.
// ---------------------------------------------------------------------------

enum class InstrKind : uint8_t {
    Alloca,
    Load,
    Store,
    LoadImm,
    BinaryOp,
    UnaryOp,
    Compare,
    Call,
    Move,
    Phi,
    Branch,
    CondBranch,
    Return,
    GlobalAddr
};

// ---------------------------------------------------------------------------
// Instr — abstract base.
// ---------------------------------------------------------------------------

class Instr {
public:
    virtual ~Instr() = default;

    virtual Value* result() const { return nullptr; }
    virtual std::vector<Value*> operands() const { return {}; }
    virtual bool is_terminator() const { return false; }
    virtual bool has_side_effect() const { return false; }
    virtual InstrKind kind() const = 0;
};

// ===========================================================================
// Concrete instruction classes
// ===========================================================================

// ---------------------------------------------------------------------------
// Alloca — allocate a stack slot for a local variable.
// result: the LocalVar that this slot represents.
// ---------------------------------------------------------------------------

class AllocaInstr : public Instr {
public:
    explicit AllocaInstr(LocalVar* result)
        : result_(result) {}

    Value* result() const override { return result_; }
    InstrKind kind() const override { return InstrKind::Alloca; }
    bool has_side_effect() const override { return true; }

    LocalVar* local() const { return result_; }

private:
    LocalVar* result_;
};

// ---------------------------------------------------------------------------
// Load — read a value from memory.
// result: Temp holding the loaded value.
// operands: [address]
// ---------------------------------------------------------------------------

class LoadInstr : public Instr {
public:
    LoadInstr(Temp* result, Value* address)
        : result_(result), address_(address) {}

    Value* result() const override { return result_; }
    std::vector<Value*> operands() const override { return {address_}; }
    InstrKind kind() const override { return InstrKind::Load; }

    Temp* temp() const { return result_; }
    Value* address() const { return address_; }

private:
    Temp* result_;
    Value* address_;
};

// ---------------------------------------------------------------------------
// Store — write a value to memory.
// result: none.
// operands: [value, address]
// ---------------------------------------------------------------------------

class StoreInstr : public Instr {
public:
    StoreInstr(Value* value, Value* address)
        : value_(value), address_(address) {}

    std::vector<Value*> operands() const override { return {value_, address_}; }
    InstrKind kind() const override { return InstrKind::Store; }
    bool has_side_effect() const override { return true; }

    Value* value() const { return value_; }
    Value* address() const { return address_; }

private:
    Value* value_;
    Value* address_;
};

// ---------------------------------------------------------------------------
// LoadImm — load an immediate integer into a temp.
// result: Temp.
// operands: [constant]
// ---------------------------------------------------------------------------

class LoadImmInstr : public Instr {
public:
    LoadImmInstr(Temp* result, Constant* constant)
        : result_(result), constant_(constant) {}

    Value* result() const override { return result_; }
    std::vector<Value*> operands() const override { return {constant_}; }
    InstrKind kind() const override { return InstrKind::LoadImm; }

    Temp* temp() const { return result_; }
    Constant* constant() const { return constant_; }

    int32_t int_value() const { return constant_->value(); }

private:
    Temp* result_;
    Constant* constant_;
};

// ---------------------------------------------------------------------------
// BinaryOp — arithmetic binary operation.
// result: Temp.
// operands: [left, right]
// ---------------------------------------------------------------------------

class BinaryOpInstr : public Instr {
public:
    enum class Op : uint8_t { Add, Sub, Mul, Div, Mod };

    BinaryOpInstr(Temp* result, Op op, Value* left, Value* right)
        : result_(result), op_(op), left_(left), right_(right) {}

    Value* result() const override { return result_; }
    std::vector<Value*> operands() const override { return {left_, right_}; }
    InstrKind kind() const override { return InstrKind::BinaryOp; }

    Temp* temp() const { return result_; }
    Op op() const { return op_; }
    Value* left() const { return left_; }
    Value* right() const { return right_; }

    static const char* op_name(Op op);

private:
    Temp* result_;
    Op op_;
    Value* left_;
    Value* right_;
};

// ---------------------------------------------------------------------------
// UnaryOp — unary operation (negation, logical not).
// result: Temp.
// operands: [value]
// ---------------------------------------------------------------------------

class UnaryOpInstr : public Instr {
public:
    enum class Op : uint8_t { Neg, Not };

    UnaryOpInstr(Temp* result, Op op, Value* value)
        : result_(result), op_(op), value_(value) {}

    Value* result() const override { return result_; }
    std::vector<Value*> operands() const override { return {value_}; }
    InstrKind kind() const override { return InstrKind::UnaryOp; }

    Temp* temp() const { return result_; }
    Op op() const { return op_; }
    Value* value() const { return value_; }

    static const char* op_name(Op op);

private:
    Temp* result_;
    Op op_;
    Value* value_;
};

// ---------------------------------------------------------------------------
// Compare — comparison predicate.
// result: Temp (0 or 1).
// operands: [left, right]
// ---------------------------------------------------------------------------

class CompareInstr : public Instr {
public:
    enum class Predicate : uint8_t { Lt, Gt, Le, Ge, Eq, Ne };

    CompareInstr(Temp* result, Predicate pred, Value* left, Value* right)
        : result_(result), pred_(pred), left_(left), right_(right) {}

    Value* result() const override { return result_; }
    std::vector<Value*> operands() const override { return {left_, right_}; }
    InstrKind kind() const override { return InstrKind::Compare; }

    Temp* temp() const { return result_; }
    Predicate predicate() const { return pred_; }
    Value* left() const { return left_; }
    Value* right() const { return right_; }

    static const char* pred_name(Predicate p);

private:
    Temp* result_;
    Predicate pred_;
    Value* left_;
    Value* right_;
};

// ---------------------------------------------------------------------------
// Call — function call.
// result: Temp (or null for void functions).
// operands: [args...]
// ---------------------------------------------------------------------------

class CallInstr : public Instr {
public:
    CallInstr(Temp* result, std::string callee, Type return_type, std::vector<Value*> args)
        : result_(result), callee_(std::move(callee)),
          return_type_(return_type), args_(std::move(args)) {}

    Value* result() const override { return result_; }
    std::vector<Value*> operands() const override;
    InstrKind kind() const override { return InstrKind::Call; }
    bool has_side_effect() const override { return true; }

    Temp* temp() const { return result_; }
    const std::string& callee() const { return callee_; }
    Type return_type() const { return return_type_; }
    const std::vector<Value*>& args() const { return args_; }

private:
    Temp* result_;
    std::string callee_;
    Type return_type_;
    std::vector<Value*> args_;
};

// ---------------------------------------------------------------------------
// Move — value copy/move between temps.
// result: Temp.
// operands: [value]
// ---------------------------------------------------------------------------

class MoveInstr : public Instr {
public:
    MoveInstr(Temp* result, Value* value)
        : result_(result), value_(value) {}

    Value* result() const override { return result_; }
    std::vector<Value*> operands() const override { return {value_}; }
    InstrKind kind() const override { return InstrKind::Move; }

    Temp* temp() const { return result_; }
    Value* value() const { return value_; }

private:
    Temp* result_;
    Value* value_;
};

// ---------------------------------------------------------------------------
// Phi — SSA phi node.
// result: Temp.
// operands: [incoming values...]
// ---------------------------------------------------------------------------

struct Incoming {
    Label* predecessor;
    Value* value;
};

class PhiInstr : public Instr {
public:
    explicit PhiInstr(Temp* result)
        : result_(result) {}

    Value* result() const override { return result_; }
    std::vector<Value*> operands() const override;
    InstrKind kind() const override { return InstrKind::Phi; }

    Temp* temp() const { return result_; }
    void add_incoming(Label* pred, Value* val);
    const std::vector<Incoming>& incoming() const { return incoming_; }

private:
    Temp* result_;
    std::vector<Incoming> incoming_;
};

// ---------------------------------------------------------------------------
// Branch — unconditional branch.
// result: none.
// operands: none.
// ---------------------------------------------------------------------------

class BranchInstr : public Instr {
public:
    explicit BranchInstr(Label* target)
        : target_(target) {}

    InstrKind kind() const override { return InstrKind::Branch; }
    bool is_terminator() const override { return true; }

    Label* target() const { return target_; }

private:
    Label* target_;
};

// ---------------------------------------------------------------------------
// CondBranch — conditional branch.
// result: none.
// operands: [condition]
// ---------------------------------------------------------------------------

class CondBranchInstr : public Instr {
public:
    CondBranchInstr(Value* condition, Label* true_target, Label* false_target)
        : condition_(condition), true_target_(true_target), false_target_(false_target) {}

    std::vector<Value*> operands() const override { return {condition_}; }
    InstrKind kind() const override { return InstrKind::CondBranch; }
    bool is_terminator() const override { return true; }

    Value* condition() const { return condition_; }
    Label* true_target() const { return true_target_; }
    Label* false_target() const { return false_target_; }

private:
    Value* condition_;
    Label* true_target_;
    Label* false_target_;
};

// ---------------------------------------------------------------------------
// Return — function return.
// result: none.
// operands: [value] (optional, empty for void return).
// ---------------------------------------------------------------------------

class ReturnInstr : public Instr {
public:
    explicit ReturnInstr(Value* value)
        : value_(value) {}

    std::vector<Value*> operands() const override;
    InstrKind kind() const override { return InstrKind::Return; }
    bool is_terminator() const override { return true; }

    Value* value() const { return value_; }
    bool has_value() const { return value_ != nullptr; }

private:
    Value* value_;
};

// ---------------------------------------------------------------------------
// GlobalAddr — compute address of a global variable.
// result: Temp.
// operands: [global]
// ---------------------------------------------------------------------------

class GlobalAddrInstr : public Instr {
public:
    GlobalAddrInstr(Temp* result, GlobalVar* global)
        : result_(result), global_(global) {}

    Value* result() const override { return result_; }
    std::vector<Value*> operands() const override { return {global_}; }
    InstrKind kind() const override { return InstrKind::GlobalAddr; }

    Temp* temp() const { return result_; }
    GlobalVar* global() const { return global_; }

private:
    Temp* result_;
    GlobalVar* global_;
};
