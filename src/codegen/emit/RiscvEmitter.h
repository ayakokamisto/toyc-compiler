#pragma once

#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>

namespace toyc::codegen {

class RiscvEmitter {
public:
    void blankLine();
    void comment(std::string_view text);
    void section(std::string_view name);
    void directive(std::string_view name, std::string_view operand = {});
    void global(std::string_view symbol);
    void label(std::string_view symbol);
    void instruction(std::string_view mnemonic,
                     std::initializer_list<std::string_view> operands = {},
                     std::string_view trailingComment = {});

    [[nodiscard]] std::string str() const;

private:
    std::ostringstream out_;
};

} // namespace toyc::codegen
