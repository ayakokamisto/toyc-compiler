#include "codegen/emit/RiscvEmitter.h"

namespace toyc::codegen {

void RiscvEmitter::blankLine() {
    out_ << '\n';
}

void RiscvEmitter::comment(std::string_view text) {
    out_ << "    # " << text << '\n';
}

void RiscvEmitter::section(std::string_view name) {
    out_ << "    " << name << '\n';
}

void RiscvEmitter::directive(std::string_view name, std::string_view operand) {
    out_ << "    " << name;
    if (!operand.empty()) {
        out_ << ' ' << operand;
    }
    out_ << '\n';
}

void RiscvEmitter::global(std::string_view symbol) {
    directive(".global", symbol);
}

void RiscvEmitter::label(std::string_view symbol) {
    out_ << symbol << ":\n";
}

void RiscvEmitter::instruction(std::string_view mnemonic,
                               std::initializer_list<std::string_view> operands,
                               std::string_view trailingComment) {
    out_ << "    " << mnemonic;
    if (operands.size() != 0) {
        out_ << ' ';
        bool first = true;
        for (std::string_view operand : operands) {
            if (!first) {
                out_ << ", ";
            }
            first = false;
            out_ << operand;
        }
    }
    if (!trailingComment.empty()) {
        out_ << "    # " << trailingComment;
    }
    out_ << '\n';
}

std::string RiscvEmitter::str() const {
    return out_.str();
}

} // namespace toyc::codegen
