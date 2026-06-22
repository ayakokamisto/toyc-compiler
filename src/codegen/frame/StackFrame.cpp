#include "codegen/frame/StackFrame.h"

#include <stdexcept>

namespace toyc::codegen {

namespace {

constexpr int kWordBytes = 4;
constexpr int kBaseSavedRegisterCount = 2; // ra and s0

void requireMutable(bool finalized) {
    if (finalized) {
        throw std::logic_error("cannot mutate finalized stack frame");
    }
}

} // namespace

void StackFrame::addVReg(std::string_view vreg) {
    requireMutable(finalized_);
    if (vreg.empty()) {
        throw std::invalid_argument("vreg name must not be empty");
    }
    if (vregIndex_.find(vreg) != vregIndex_.end()) {
        return;
    }
    vregIndex_.emplace(std::string(vreg), vregs_.size());
    vregs_.emplace_back(vreg);
}

void StackFrame::addCalleeSavedRegister(std::string_view reg) {
    requireMutable(finalized_);
    if (reg.empty()) {
        throw std::invalid_argument("register name must not be empty");
    }
    if (reg == "ra" || reg == "s0") {
        return;
    }
    if (calleeSavedIndex_.find(reg) != calleeSavedIndex_.end()) {
        return;
    }
    calleeSavedIndex_.emplace(std::string(reg), calleeSavedRegs_.size());
    calleeSavedRegs_.emplace_back(reg);
}

void StackFrame::setOutgoingArgBytes(int bytes) {
    requireMutable(finalized_);
    if (bytes < 0) {
        throw std::invalid_argument("outgoing argument byte count must be non-negative");
    }
    outgoingArgBytes_ = alignTo16(bytes);
}

void StackFrame::finalize() {
    if (finalized_) {
        return;
    }

    savedRegisterSlots_.clear();
    vregSlots_.clear();
    vregOffsets_.clear();

    const int savedRegisterBytes =
        (kBaseSavedRegisterCount + static_cast<int>(calleeSavedRegs_.size())) * kWordBytes;
    const int vregBytes = static_cast<int>(vregs_.size()) * kWordBytes;
    frameSizeBytes_ = alignTo16(savedRegisterBytes + vregBytes);

    int offsetFromS0 = -kWordBytes;
    savedRegisterSlots_.push_back({"ra", offsetFromS0, frameSizeBytes_ + offsetFromS0});
    offsetFromS0 -= kWordBytes;
    savedRegisterSlots_.push_back({"s0", offsetFromS0, frameSizeBytes_ + offsetFromS0});

    for (const std::string& reg : calleeSavedRegs_) {
        offsetFromS0 -= kWordBytes;
        savedRegisterSlots_.push_back({reg, offsetFromS0, frameSizeBytes_ + offsetFromS0});
    }

    for (const std::string& vreg : vregs_) {
        offsetFromS0 -= kWordBytes;
        vregSlots_.push_back({vreg, offsetFromS0});
        vregOffsets_.emplace(vreg, offsetFromS0);
    }

    finalized_ = true;
}

bool StackFrame::containsVReg(std::string_view vreg) const {
    return vregIndex_.find(vreg) != vregIndex_.end();
}

int StackFrame::vregOffsetFromS0(std::string_view vreg) const {
    const auto it = vregOffsets_.find(vreg);
    if (it == vregOffsets_.end()) {
        throw std::out_of_range("unknown vreg stack slot");
    }
    return it->second;
}

int StackFrame::frameSizeBytes() const {
    return frameSizeBytes_;
}

int StackFrame::outgoingArgBytes() const {
    return outgoingArgBytes_;
}

const std::vector<StackSlot>& StackFrame::vregSlots() const {
    return vregSlots_;
}

const std::vector<SavedRegisterSlot>& StackFrame::savedRegisterSlots() const {
    return savedRegisterSlots_;
}

int StackFrame::alignTo16(int bytes) {
    return (bytes + 15) & ~15;
}

} // namespace toyc::codegen
