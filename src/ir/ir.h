#pragma once

#include "common/diagnostic.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace toyc::ast {
struct CompUnit;
}

namespace toyc::sema {
class SemanticModel;
}

namespace toyc::ir {

enum class Type {
    I32,
    Void,
};

using ValueId = std::uint32_t;
using BlockId = std::uint32_t;

struct Immediate {
    std::int32_t value = 0;
};

struct Value {
    ValueId id = 0;
};

struct GlobalRef {
    std::string name;
};

using Operand = std::variant<Immediate, Value, GlobalRef>;

enum class BinaryOp {
    Add,
    Subtract,
    Multiply,
    Divide,
    Remainder,
};

enum class CompareOp {
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    Equal,
    NotEqual,
};

struct BinaryInst {
    ValueId result = 0;
    BinaryOp op = BinaryOp::Add;
    Operand left{};
    Operand right{};
};

struct CompareInst {
    ValueId result = 0;
    CompareOp op = CompareOp::Equal;
    Operand left{};
    Operand right{};
};

struct UnaryNotInst {
    ValueId result = 0;
    Operand operand{};
};

struct AllocaInst {
    ValueId result = 0;
};

struct LoadInst {
    ValueId result = 0;
    Operand address{};
};

struct StoreInst {
    Operand address{};
    Operand value{};
};

struct CallInst {
    std::optional<ValueId> result;
    std::string callee;
    std::vector<Operand> arguments;
};

struct JumpInst {
    BlockId target = 0;
};

struct BranchInst {
    Operand condition{};
    BlockId trueTarget = 0;
    BlockId falseTarget = 0;
};

struct ReturnInst {
    std::optional<Operand> value;
};

using Instruction = std::variant<BinaryInst, CompareInst, UnaryNotInst, AllocaInst, LoadInst,
                                 StoreInst, CallInst, JumpInst, BranchInst, ReturnInst>;

struct Parameter {
    std::uint32_t index = 0;
    std::string name;
    Type type = Type::I32;
};

struct BasicBlock {
    BlockId id = 0;
    std::string name;
    std::vector<Instruction> instructions;
};

struct Function {
    std::string name;
    Type returnType = Type::Void;
    std::vector<Parameter> parameters;
    std::vector<BasicBlock> basicBlocks;
};

struct Global {
    std::string name;
    bool isConst = false;
    std::int32_t initializer = 0;
};

struct Module {
    std::vector<Global> globals;
    std::vector<Function> functions;
};

class IRGenerator {
public:
    Module generate(const ast::CompUnit& unit, const sema::SemanticModel& semanticModel);

    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept;

private:
    std::vector<Diagnostic> diagnostics_;
};

bool verifyModule(const Module& module, std::vector<Diagnostic>& diagnostics);

} // namespace toyc::ir
