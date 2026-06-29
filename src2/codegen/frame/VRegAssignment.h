#pragma once

#include <map>
#include <optional>
#include <cstdint>
#include <string>
#include <string_view>

namespace toyc::codegen {

class VRegAssignment {
public:
    void assignPhysical(std::string_view vreg, std::string_view reg);
    void assignRematerializedConstant(std::string_view vreg, std::int32_t value);

    [[nodiscard]] bool isStackSlot(std::string_view vreg) const;
    [[nodiscard]] std::optional<std::string_view> physicalReg(std::string_view vreg) const;
    [[nodiscard]] std::optional<std::int32_t> rematerializedConstant(std::string_view vreg) const;

private:
    std::map<std::string, std::string, std::less<>> physical_;
    std::map<std::string, std::int32_t, std::less<>> rematerializedConstants_;
};

} // namespace toyc::codegen
