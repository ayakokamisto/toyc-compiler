#include "toyc/backend/riscv_emitter.h"
#include <cctype>
#include <sstream>
#include <stdexcept>

RiscvEmitter::RiscvEmitter(std::ostream& os) : os_(os) {}
std::string RiscvEmitter::emit(const IRProgram& program) {
    std::ostringstream os; RiscvEmitter emitter(os);
    emitter.emit_module(*program.module()); return os.str();
}

static std::string load_val(const FrameLayout& layout, Value* val, const std::string& reg) {
    if (auto* c = dynamic_cast<Constant*>(val))
        return "li " + reg + ", " + std::to_string(c->value());
    auto offset = layout.valueOffset(val);
    if (!offset)
        throw std::runtime_error("value has no frame home");
    return "lw " + reg + ", " + std::to_string(*offset) + "(s0)";
}
static std::string store_val(const FrameLayout& layout, Value* val, const std::string& reg) {
    auto offset = layout.valueOffset(val);
    if (!offset)
        throw std::runtime_error("value has no frame home");
    return "sw " + reg + ", " + std::to_string(*offset) + "(s0)";
}

static std::string escape_label_component(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
        if (std::isalnum(ch) || ch == '_') out.push_back(static_cast<char>(ch));
        else out.push_back('_');
    }
    if (out.empty()) out = "block";
    return out;
}

void RiscvEmitter::emit_module(const Module& mod) {
    if (!mod.globals().empty()) {
        os_ << ".data\n";
        for (auto* gv : mod.globals()) {
            os_ << ".global " << gv->symbol_name() << "\n" << gv->symbol_name() << ":\n";
            os_ << "  .word " << gv->initial_value() << "\n";
        }
    }
    os_ << ".text\n";
    for (const auto& fn : mod.functions()) emit_function(*fn);
}

void RiscvEmitter::emit_function(const Function& fn) {
    layout_ = FrameLayout::compute(fn);
    currentFunction_ = &fn;
    prepareBlockLabels(fn);
    os_ << ".global " << fn.name() << "\n" << fn.name() << ":\n";
    auto& blocks = fn.blocks();
    if (!blocks.empty()) os_ << blockLabel(fn, *blocks[0]) << ":\n";
    emit_prologue(layout_, fn);
    for (size_t i = 0; i < fn.parameters().size() && i < 8; i++) {
        auto offset = layout_.allocaOffset(fn.parameters()[i]);
        if (offset)
            os_ << "  sw a" << i << ", " << *offset << "(s0)\n";
    }
    for (size_t i = 8; i < fn.parameters().size(); i++) {
        auto offset = layout_.allocaOffset(fn.parameters()[i]);
        if (offset)
            os_ << "lw t0, " << layout_.incomingStackArgOffset((uint32_t)i) << "(s0)\n  sw t0, " << *offset << "(s0)\n";
    }
    bool first = true;
    for (const auto& bb : fn.blocks()) {
        if (first) { first = false;
            for (auto* instr : bb->all_instrs())
                { os_ << "  "; emit_instr(*instr, fn); os_ << "\n"; }
        } else emit_block(*bb, fn);
    }
    currentFunction_ = nullptr;
}

void RiscvEmitter::prepareBlockLabels(const Function& function) {
    blockLabels_.clear();
    const std::string functionName = escape_label_component(function.name());
    size_t ordinal = 0;
    for (const auto& block : function.blocks()) {
        std::string blockName = escape_label_component(block->label()->name());
        blockLabels_[block->label()] = ".L" + functionName + "__" + blockName + "_" + std::to_string(ordinal++);
    }
}

std::string RiscvEmitter::blockLabel(const Function& function, const BasicBlock& block) {
    return blockLabel(function, block.label());
}

std::string RiscvEmitter::blockLabel(const Function&, const Label* label) {
    auto it = blockLabels_.find(label);
    if (it == blockLabels_.end()) throw std::runtime_error("missing block label");
    return it->second;
}

void RiscvEmitter::emit_block(const BasicBlock& bb, const Function& fn) {
    os_ << blockLabel(fn, bb) << ":\n";
    for (auto* instr : bb.all_instrs())
        { os_ << "  "; emit_instr(*instr, fn); os_ << "\n"; }
}

void RiscvEmitter::emit_instr(const Instr& instr, const Function& fn) {
    switch (instr.kind()) {
    case InstrKind::Alloca: return;
    case InstrKind::LoadImm: emit_load_imm(static_cast<const LoadImmInstr&>(instr)); return;
    case InstrKind::BinaryOp: emit_binary_op(static_cast<const BinaryOpInstr&>(instr)); return;
    case InstrKind::UnaryOp: emit_unary_op(static_cast<const UnaryOpInstr&>(instr)); return;
    case InstrKind::Compare: emit_compare(static_cast<const CompareInstr&>(instr)); return;
    case InstrKind::Store: emit_store(static_cast<const StoreInstr&>(instr)); return;
    case InstrKind::Load: emit_load(static_cast<const LoadInstr&>(instr)); return;
    case InstrKind::Branch: emit_branch(static_cast<const BranchInstr&>(instr)); return;
    case InstrKind::CondBranch: emit_cond_branch(static_cast<const CondBranchInstr&>(instr)); return;
    case InstrKind::Return: emit_return(static_cast<const ReturnInstr&>(instr)); return;
    case InstrKind::Call: emit_call_instr(static_cast<const CallInstr&>(instr)); return;
    case InstrKind::GlobalAddr: emit_global_addr(static_cast<const GlobalAddrInstr&>(instr)); return;
    case InstrKind::Phi: throw std::runtime_error("P2 emitter does not support Phi");
    case InstrKind::Move: throw std::runtime_error("P2 emitter does not support Move");
    }
}

void RiscvEmitter::emit_prologue(const FrameLayout& layout, const Function&) {
    if (layout.frameSize == 0) return;
    os_ << "addi sp, sp, -" << layout.frameSize << "\n";
    os_ << "  sw ra, " << layout.raOffset << "(sp)\n";
    os_ << "  sw s0, " << layout.s0Offset << "(sp)\n";
    os_ << "  mv s0, sp\n";
}
void RiscvEmitter::emit_epilogue(const FrameLayout& layout) {
    if (layout.frameSize == 0) { os_ << "ret"; return; }
    os_ << "mv sp, s0\n  lw ra, " << layout.raOffset << "(sp)\n";
    os_ << "  lw s0, " << layout.s0Offset << "(sp)\n";
    os_ << "  addi sp, sp, " << layout.frameSize << "\n  ret";
}

void RiscvEmitter::emit_load_imm(const LoadImmInstr& li) {
    os_ << "li t0, " << li.int_value() << "\n  " << store_val(layout_, li.result(), "t0");
}
void RiscvEmitter::emit_binary_op(const BinaryOpInstr& bin) {
    os_ << load_val(layout_, bin.left(), "t0") << "\n  " << load_val(layout_, bin.right(), "t1") << "\n";
    const char* op = "";
    switch (bin.op()) {
    case BinaryOpInstr::Op::Add: op = "add"; break;
    case BinaryOpInstr::Op::Sub: op = "sub"; break;
    case BinaryOpInstr::Op::Mul: op = "mul"; break;
    case BinaryOpInstr::Op::Div: op = "div"; break;
    case BinaryOpInstr::Op::Mod: op = "rem"; break;
    }
    os_ << "  " << op << " t2, t0, t1\n  " << store_val(layout_, bin.result(), "t2");
}
void RiscvEmitter::emit_unary_op(const UnaryOpInstr& un) {
    os_ << load_val(layout_, un.value(), "t0") << "\n";
    switch (un.op()) {
    case UnaryOpInstr::Op::Neg: os_ << "  neg t2, t0\n"; break;
    case UnaryOpInstr::Op::Not: os_ << "  seqz t2, t0\n"; break;
    }
    os_ << "  " << store_val(layout_, un.result(), "t2");
}
void RiscvEmitter::emit_compare(const CompareInstr& cmp) {
    os_ << load_val(layout_, cmp.left(), "t0") << "\n  " << load_val(layout_, cmp.right(), "t1") << "\n";
    switch (cmp.predicate()) {
    case CompareInstr::Predicate::Lt: os_ << "  slt t2, t0, t1\n"; break;
    case CompareInstr::Predicate::Gt: os_ << "  slt t2, t1, t0\n"; break;
    case CompareInstr::Predicate::Le: os_ << "  slt t2, t1, t0\n  xori t2, t2, 1\n"; break;
    case CompareInstr::Predicate::Ge: os_ << "  slt t2, t0, t1\n  xori t2, t2, 1\n"; break;
    case CompareInstr::Predicate::Eq: os_ << "  xor t2, t0, t1\n  seqz t2, t2\n"; break;
    case CompareInstr::Predicate::Ne: os_ << "  xor t2, t0, t1\n  snez t2, t2\n"; break;
    }
    os_ << "  " << store_val(layout_, cmp.result(), "t2");
}
void RiscvEmitter::emit_store(const StoreInstr& st) {
    os_ << load_val(layout_, st.value(), "t0") << "\n";
    auto alloca = layout_.allocaOffset(st.address());
    if (alloca) { os_ << "  sw t0, " << *alloca << "(s0)"; return; }
    auto value = layout_.valueOffset(st.address());
    if (value) {
        os_ << "  lw t1, " << *value << "(s0)\n  sw t0, 0(t1)"; return;
    }
    throw std::runtime_error("P2 emitter only supports local alloca or global addr for Store");
}
void RiscvEmitter::emit_load(const LoadInstr& ld) {
    auto alloca = layout_.allocaOffset(ld.address());
    if (alloca) { os_ << "lw t0, " << *alloca << "(s0)\n  " << store_val(layout_, ld.result(), "t0"); return; }
    auto value = layout_.valueOffset(ld.address());
    if (value) {
        os_ << "lw t0, " << *value << "(s0)\n  lw t1, 0(t0)\n  " << store_val(layout_, ld.result(), "t1"); return;
    }
    throw std::runtime_error("P2 emitter only supports local alloca or global addr for Load");
}
void RiscvEmitter::emit_branch(const BranchInstr& br) { os_ << "j " << blockLabel(*currentFunction_, br.target()); }
void RiscvEmitter::emit_cond_branch(const CondBranchInstr& cbr) {
    os_ << load_val(layout_, cbr.condition(), "t0") << "\n  bnez t0, " << blockLabel(*currentFunction_, cbr.true_target()) << "\n  j " << blockLabel(*currentFunction_, cbr.false_target());
}
void RiscvEmitter::emit_return(const ReturnInstr& ret) {
    if (ret.has_value()) { os_ << load_val(layout_, ret.value(), "a0") << "\n  "; }
    emit_epilogue(layout_);
}
void RiscvEmitter::emit_global_addr(const GlobalAddrInstr& ga) {
    os_ << "la t0, " << ga.global()->symbol_name() << "\n  " << store_val(layout_, ga.result(), "t0");
}
void RiscvEmitter::emit_call_instr(const CallInstr& call) {
    size_t n = call.args().size();
    for (size_t i = 0; i < n && i < 8; i++) {
        os_ << load_val(layout_, call.args()[i], "a" + std::to_string(i)) << "\n  ";
    }
    for (size_t i = 8; i < n; i++) {
        os_ << load_val(layout_, call.args()[i], "t0") << "\n  sw t0, " << layout_.outgoingArgOffset((uint32_t)(i - 8)) << "(s0)\n  ";
    }
    os_ << "call " << call.callee() << "\n";
    if (call.result()) { os_ << "  " << store_val(layout_, call.result(), "a0"); }
}
