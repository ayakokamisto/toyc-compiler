#pragma once

// Backend consumption view aligned with docs/contracts/ir_backend_contract.md.
// It keeps the fields and instructions codegen consumes, while omitting
// debug-only contract tables such as constTable, funcTable, and symTable.
// Member three produces this shape through src/ir/contract_ir_generator.h.

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace toyc::codegen::contract {

enum class Type {
    Int,
    Void,
};

struct GlobalObject {
    std::string name;
    std::int32_t initValue = 0;
};

struct Param {
    std::string sourceName;
    std::string vreg;
};

struct ConstInst {
    std::string dst;
    std::int32_t value = 0;
};

struct CopyInst {
    std::string dst;
    std::string src;
};

struct LoadGlobalInst {
    std::string dst;
    std::string name;
};

struct StoreGlobalInst {
    std::string name;
    std::string src;
};

struct CallInst {
    std::string dst;
    std::string functionName;
    std::vector<std::string> args;
};

struct CallVoidInst {
    std::string functionName;
    std::vector<std::string> args;
};

struct AddInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct SubInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct MulInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct DivInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct ModInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct NegInst {
    std::string dst;
    std::string src;
};

struct EqInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct NeInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct LtInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct LeInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct GtInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct GeInst {
    std::string dst;
    std::string src1;
    std::string src2;
};

struct LNotInst {
    std::string dst;
    std::string src;
};

struct JumpInst {
    std::string targetLabel;
};

struct BranchInst {
    std::string cond;
    std::string trueLabel;
    std::string falseLabel;
};

struct ReturnInst {
    std::optional<std::string> src;
};

using Instruction = std::variant<ConstInst,
                                 CopyInst,
                                 LoadGlobalInst,
                                 StoreGlobalInst,
                                 CallInst,
                                 CallVoidInst,
                                 AddInst,
                                 SubInst,
                                 MulInst,
                                 DivInst,
                                 ModInst,
                                 NegInst,
                                 EqInst,
                                 NeInst,
                                 LtInst,
                                 LeInst,
                                 GtInst,
                                 GeInst,
                                 LNotInst>;
using Terminator = std::variant<JumpInst, BranchInst, ReturnInst>;

struct BasicBlock {
    std::string label = "entry";
    std::vector<Instruction> instructions;
    Terminator terminator = ReturnInst{};
};

struct IRFunction {
    std::string name;
    Type returnType = Type::Void;
    std::vector<Param> params;
    std::vector<BasicBlock> basicBlocks;
};

struct IRModule {
    std::vector<GlobalObject> globalVars;
    std::vector<IRFunction> functions;
};

} // namespace toyc::codegen::contract
