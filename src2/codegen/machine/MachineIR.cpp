#include "codegen/machine/MachineIR.h"

#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace toyc::codegen::machine {
namespace {

std::string trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return std::string(text);
}

std::string stripComment(std::string_view line) {
    const std::size_t commentPos = line.find('#');
    if (commentPos != std::string_view::npos) {
        line = line.substr(0, commentPos);
    }
    return trim(line);
}

std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::istringstream stream{std::string(text)};
    std::string line;
    while (std::getline(stream, line)) {
        while (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

std::vector<std::string> splitOperands(std::string_view text) {
    std::vector<std::string> operands;
    while (!text.empty()) {
        const std::size_t commaPos = text.find(',');
        if (commaPos == std::string_view::npos) {
            operands.push_back(trim(text));
            break;
        }
        operands.push_back(trim(text.substr(0, commaPos)));
        text.remove_prefix(commaPos + 1);
    }
    return operands;
}

Opcode parseOpcode(std::string_view mnemonic) {
    if (mnemonic == "add") return Opcode::Add;
    if (mnemonic == "addi") return Opcode::Addi;
    if (mnemonic == "sub") return Opcode::Sub;
    if (mnemonic == "mul") return Opcode::Mul;
    if (mnemonic == "div") return Opcode::Div;
    if (mnemonic == "rem") return Opcode::Rem;
    if (mnemonic == "lw") return Opcode::Lw;
    if (mnemonic == "sw") return Opcode::Sw;
    if (mnemonic == "beq") return Opcode::Beq;
    if (mnemonic == "bne") return Opcode::Bne;
    if (mnemonic == "blt") return Opcode::Blt;
    if (mnemonic == "bge") return Opcode::Bge;
    if (mnemonic == "bnez") return Opcode::Bnez;
    if (mnemonic == "beqz") return Opcode::Beqz;
    if (mnemonic == "j") return Opcode::J;
    if (mnemonic == "call") return Opcode::Call;
    if (mnemonic == "ret") return Opcode::Ret;
    if (mnemonic == "li") return Opcode::Li;
    if (mnemonic == "la") return Opcode::La;
    if (mnemonic == "mv") return Opcode::Mv;
    if (mnemonic == "seqz") return Opcode::Seqz;
    if (mnemonic == "snez") return Opcode::Snez;
    return Opcode::Unknown;
}

bool isBarrier(const MachineInstr& instr) {
    switch (instr.opcode) {
    case Opcode::Beq:
    case Opcode::Bne:
    case Opcode::Blt:
    case Opcode::Bge:
    case Opcode::Bnez:
    case Opcode::Beqz:
    case Opcode::J:
    case Opcode::Call:
    case Opcode::Ret:
        return true;
    default:
        return false;
    }
}

std::string definedReg(const MachineInstr& instr) {
    if (instr.operands.empty()) {
        return {};
    }
    switch (instr.opcode) {
    case Opcode::Add:
    case Opcode::Addi:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Div:
    case Opcode::Rem:
    case Opcode::Lw:
    case Opcode::Li:
    case Opcode::La:
    case Opcode::Mv:
    case Opcode::Seqz:
    case Opcode::Snez:
        return instr.operands[0];
    default:
        return {};
    }
}

std::string formatMv(const std::string& dst, const std::string& src) {
    return "    mv " + dst + ", " + src;
}

std::string joinLines(const std::vector<std::string>& lines) {
    std::ostringstream out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        out << lines[i];
        if (i + 1 < lines.size()) {
            out << '\n';
        }
    }
    if (!lines.empty()) {
        out << '\n';
    }
    return out.str();
}

} // namespace

MachineInstr parseMachineLine(std::string_view line) {
    MachineInstr instr;
    instr.originalLine = std::string(line);

    const std::string text = stripComment(line);
    if (text.empty() || text.back() == ':' || text.front() == '.') {
        return instr;
    }

    std::string_view view(text);
    const std::size_t spacePos = view.find(' ');
    const std::string mnemonic =
        spacePos == std::string_view::npos ? std::string(view) : std::string(view.substr(0, spacePos));
    instr.opcode = parseOpcode(mnemonic);
    instr.isInstruction = true;
    if (spacePos != std::string_view::npos) {
        instr.operands = splitOperands(view.substr(spacePos + 1));
    }
    return instr;
}

std::string optimizeStackForwarding(std::string assembly) {
    struct SlotValue {
        std::string reg;
    };

    std::vector<std::string> lines = splitLines(assembly);
    std::vector<std::string> output;
    output.reserve(lines.size());

    std::unordered_map<std::string, SlotValue> stackSlots;
    for (const std::string& line : lines) {
        MachineInstr instr = parseMachineLine(line);
        if (!instr.isInstruction) {
            stackSlots.clear();
            output.push_back(line);
            continue;
        }

        if (isBarrier(instr)) {
            stackSlots.clear();
            output.push_back(line);
            continue;
        }

        const std::string def = definedReg(instr);
        if (!def.empty()) {
            for (auto it = stackSlots.begin(); it != stackSlots.end();) {
                if (it->second.reg == def) {
                    it = stackSlots.erase(it);
                } else {
                    ++it;
                }
            }
        }

        if (instr.opcode == Opcode::Lw && instr.operands.size() == 2) {
            const auto slotIt = stackSlots.find(instr.operands[1]);
            if (slotIt != stackSlots.end()) {
                if (instr.operands[0] != slotIt->second.reg) {
                    output.push_back(formatMv(instr.operands[0], slotIt->second.reg));
                }
                continue;
            }
        }

        output.push_back(line);

        if (instr.opcode == Opcode::Sw && instr.operands.size() == 2) {
            stackSlots[instr.operands[1]] = SlotValue{instr.operands[0]};
        }
    }

    return joinLines(output);
}

} // namespace toyc::codegen::machine
