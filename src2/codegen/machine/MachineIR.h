#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace toyc::codegen::machine {

enum class Opcode {
    Unknown,
    Add,
    Addi,
    Sub,
    Mul,
    Div,
    Rem,
    Lw,
    Sw,
    Beq,
    Bne,
    Blt,
    Bge,
    Bnez,
    Beqz,
    J,
    Call,
    Ret,
    Li,
    La,
    Mv,
    Seqz,
    Snez,
};

struct MachineInstr {
    Opcode opcode = Opcode::Unknown;
    std::string originalLine;
    std::vector<std::string> operands;
    bool isInstruction = false;
};

[[nodiscard]] MachineInstr parseMachineLine(std::string_view line);
[[nodiscard]] std::string optimizeStackForwarding(std::string assembly);

} // namespace toyc::codegen::machine
