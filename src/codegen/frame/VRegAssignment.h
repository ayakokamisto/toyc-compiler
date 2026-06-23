#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace toyc::codegen {

class VRegAssignment {
public:
    void assignPhysical(std::string_view vreg, std::string_view reg);

    [[nodiscard]] bool isStackSlot(std::string_view vreg) const;
    [[nodiscard]] std::optional<std::string_view> physicalReg(std::string_view vreg) const;

private:
    std::map<std::string, std::string, std::less<>> physical_;
};

} // namespace toyc::codegen
