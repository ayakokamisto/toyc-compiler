#pragma once
#include "frame_layout.h"
#include "toyc/ir/module.h"
#include <ostream>
#include <string>
#include <unordered_map>

class RiscvEmitter {
public:
    static std::string emit(const IRProgram& program);
private:
    explicit RiscvEmitter(std::ostream& os);
    void emit_module(const Module& mod);
    void emit_function(const Function& fn);
    void emit_block(const BasicBlock& bb, const Function& fn);
    void emit_instr(const Instr& instr, const Function& fn);
    void emit_load_imm(const LoadImmInstr& li);
    void emit_binary_op(const BinaryOpInstr& bin);
    void emit_unary_op(const UnaryOpInstr& un);
    void emit_compare(const CompareInstr& cmp);
    void emit_store(const StoreInstr& st);
    void emit_load(const LoadInstr& ld);
    void emit_branch(const BranchInstr& br);
    void emit_cond_branch(const CondBranchInstr& cbr);
    void emit_return(const ReturnInstr& ret);
    void emit_call_instr(const CallInstr& call);
    void emit_global_addr(const GlobalAddrInstr& ga);
    void emit_prologue(const FrameLayout& layout, const Function& fn);
    void emit_epilogue(const FrameLayout& layout);
    std::string blockLabel(const Function& function, const BasicBlock& block);
    std::string blockLabel(const Function& function, const Label* label);
    void prepareBlockLabels(const Function& function);
    FrameLayout layout_;
    std::unordered_map<const Label*, std::string> blockLabels_;
    const Function* currentFunction_ = nullptr;
    std::ostream& os_;
};
