#include "codegen/frame/VRegAssignment.h"

namespace toyc::codegen {

void VRegAssignment::assignPhysical(std::string_view vreg, std::string_view reg) {
    physical_.emplace(std::string(vreg), std::string(reg));
}

bool VRegAssignment::isStackSlot(std::string_view vreg) const {
    return physical_.find(vreg) == physical_.end();
}

std::optional<std::string_view> VRegAssignment::physicalReg(std::string_view vreg) const {
    const auto it = physical_.find(vreg);
    if (it == physical_.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace toyc::codegen
