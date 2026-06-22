#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace toyc::codegen {

struct StackSlot {
    std::string name;
    int offsetFromS0 = 0;
};

struct SavedRegisterSlot {
    std::string reg;
    int offsetFromS0 = 0;
    int offsetFromSp = 0;
};

class StackFrame {
public:
    void addVReg(std::string_view vreg);
    void addCalleeSavedRegister(std::string_view reg);
    void setOutgoingArgBytes(int bytes);
    void finalize();

    [[nodiscard]] bool containsVReg(std::string_view vreg) const;
    [[nodiscard]] int vregOffsetFromS0(std::string_view vreg) const;
    [[nodiscard]] int frameSizeBytes() const;
    [[nodiscard]] int outgoingArgBytes() const;
    [[nodiscard]] const std::vector<StackSlot>& vregSlots() const;
    [[nodiscard]] const std::vector<SavedRegisterSlot>& savedRegisterSlots() const;

private:
    [[nodiscard]] static int alignTo16(int bytes);

    bool finalized_ = false;
    int outgoingArgBytes_ = 0;
    int frameSizeBytes_ = 0;
    std::vector<std::string> vregs_;
    std::vector<std::string> calleeSavedRegs_;
    std::vector<StackSlot> vregSlots_;
    std::vector<SavedRegisterSlot> savedRegisterSlots_;
    std::map<std::string, std::size_t, std::less<>> vregIndex_;
    std::map<std::string, std::size_t, std::less<>> calleeSavedIndex_;
    std::map<std::string, int, std::less<>> vregOffsets_;
};

} // namespace toyc::codegen
