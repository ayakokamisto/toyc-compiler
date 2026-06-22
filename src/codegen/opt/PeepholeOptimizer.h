#pragma once

#include <string>
#include <string_view>

namespace toyc::codegen {

class PeepholeOptimizer {
public:
    [[nodiscard]] static std::string optimize(std::string assembly);
};

} // namespace toyc::codegen
