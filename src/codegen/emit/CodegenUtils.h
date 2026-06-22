#pragma once

#include <string>
#include <string_view>

namespace toyc::codegen {

inline std::string imm(int value) {
    return std::to_string(value);
}

inline std::string offsetReg(int offset, std::string_view reg) {
    return std::to_string(offset) + "(" + std::string(reg) + ")";
}

inline std::string globalLabel(std::string_view name) {
    if (!name.empty() && name.front() == '@') {
        return std::string(name.substr(1));
    }
    return std::string(name);
}

inline std::string blockLabel(std::string_view functionName, std::string_view blockName) {
    if (blockName == "entry") {
        return std::string(functionName);
    }
    return std::string(functionName) + "__" + std::string(blockName);
}

inline std::string epilogueLabel(std::string_view functionName) {
    return ".L" + std::string(functionName) + "__epilogue";
}

inline int alignTo16(int bytes) {
    return (bytes + 15) & ~15;
}

} // namespace toyc::codegen
