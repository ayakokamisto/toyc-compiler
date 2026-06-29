#include "codegen/frame/VRegAssignment.h"

namespace toyc::codegen {

void VRegAssignment::assignPhysical(std::string_view vreg, std::string_view reg) {
    physical_.emplace(std::string(vreg), std::string(reg));
}

void VRegAssignment::assignRematerializedConstant(std::string_view vreg, const std::int32_t value) {
    rematerializedConstants_[std::string(vreg)] = value;
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

std::optional<std::int32_t> VRegAssignment::rematerializedConstant(std::string_view vreg) const {
    const auto it = rematerializedConstants_.find(vreg);
    if (it == rematerializedConstants_.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace toyc::codegen
