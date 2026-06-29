#include "codegen/opt/PeepholeOptimizer.h"

#include "codegen/machine/MachineIR.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace toyc::codegen {

namespace {

std::string stripCarriageReturn(std::string line) {
    while (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return line;
}

std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::istringstream stream{std::string(text)};
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(stripCarriageReturn(std::move(line)));
    }
    return lines;
}

bool isBlankOrComment(std::string_view line) {
    if (line.empty()) {
        return true;
    }
    for (char ch : line) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            return ch == '#';
        }
    }
    return true;
}

bool isLabelOrDirective(std::string_view line) {
    if (line.empty() || std::isspace(static_cast<unsigned char>(line.front()))) {
        return false;
    }
    // Labels end with ':', directives start with '.' or are function-level markers.
    return line.ends_with(':') || line.front() == '.';
}

bool isControlFlow(std::string_view line) {
    // Matches: j, bnez, beqz, blt, bge, bne, beq, ret, call, jr, jalr
    constexpr std::string_view prefixes[] = {
        "    j ",   "    bnez ", "    beqz ", "    blt ",  "    bge ",
        "    bne ", "    beq ",  "    ret",    "    call ", "    jr ",
        "    jalr ",
    };
    for (const auto& p : prefixes) {
        if (line.starts_with(p)) {
            return true;
        }
    }
    return line == "    ret";
}

bool parseUnconditionalJump(std::string_view line, std::string& target) {
    constexpr std::string_view prefix = "    j ";
    if (!line.starts_with(prefix)) {
        return false;
    }
    target = std::string(line.substr(prefix.size()));
    const std::size_t commentPos = target.find('#');
    if (commentPos != std::string::npos) {
        target = target.substr(0, commentPos);
    }
    while (!target.empty() && std::isspace(static_cast<unsigned char>(target.back()))) {
        target.pop_back();
    }
    return !target.empty();
}

bool isLabelLine(std::string_view line, std::string_view label) {
    if (line.empty() || std::isspace(static_cast<unsigned char>(line.front()))) {
        return false;
    }
    if (!line.ends_with(':')) {
        return false;
    }
    return line.substr(0, line.size() - 1) == label;
}

bool isRedundantMove(std::string_view line) {
    constexpr std::string_view prefix = "    mv ";
    if (!line.starts_with(prefix)) {
        return false;
    }
    std::string_view rest = line.substr(prefix.size());
    const std::size_t commaPos = rest.find(',');
    if (commaPos == std::string_view::npos) {
        return false;
    }
    std::string_view dst = rest.substr(0, commaPos);
    std::string_view src = rest.substr(commaPos + 1);
    while (!src.empty() && std::isspace(static_cast<unsigned char>(src.front()))) {
        src.remove_prefix(1);
    }
    const std::size_t commentPos = src.find('#');
    if (commentPos != std::string_view::npos) {
        src = src.substr(0, commentPos);
    }
    while (!src.empty() && std::isspace(static_cast<unsigned char>(src.back()))) {
        src.remove_suffix(1);
    }
    return dst == src;
}

// Trim whitespace and trailing comments from a register name.
std::string trimReg(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    const std::size_t commentPos = sv.find('#');
    if (commentPos != std::string_view::npos) {
        sv = sv.substr(0, commentPos);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return std::string(sv);
}

std::string stripInlineComment(std::string_view line) {
    std::string text(line);
    const std::size_t commentPos = text.find('#');
    if (commentPos != std::string::npos) {
        text = text.substr(0, commentPos);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    return text;
}

bool registerTokenAppears(std::string_view text, std::string_view reg) {
    std::size_t pos = 0;
    while ((pos = text.find(reg, pos)) != std::string_view::npos) {
        const bool startOk =
            pos == 0 ||
            (!std::isalnum(static_cast<unsigned char>(text[pos - 1])) &&
             text[pos - 1] != '_');
        const std::size_t end = pos + reg.size();
        const bool endOk =
            end >= text.size() ||
            (!std::isalnum(static_cast<unsigned char>(text[end])) &&
             text[end] != '_');
        if (startOk && endOk) {
            return true;
        }
        ++pos;
    }
    return false;
}

// Parse "mv dst, src" from a line. Returns false if not a mv instruction.
bool parseMv(std::string_view line, std::string& dst, std::string& src) {
    constexpr std::string_view prefix = "    mv ";
    if (!line.starts_with(prefix)) {
        return false;
    }
    std::string_view rest = line.substr(prefix.size());
    const std::size_t commaPos = rest.find(',');
    if (commaPos == std::string_view::npos) {
        return false;
    }
    dst = trimReg(rest.substr(0, commaPos));
    src = trimReg(rest.substr(commaPos + 1));
    return !dst.empty() && !src.empty();
}

// Check if a line is a 2-register + immediate ALU instruction: mnemonic rd, rs, imm.
// Returns the mnemonic, rd, rs, imm if so.
bool parseAlu2Imm(std::string_view line,
                  std::string& mnemonic,
                  std::string& rd,
                  std::string& rs,
                  std::string& immStr) {
    if (line.size() < 5 || line.substr(0, 4) != "    ") {
        return false;
    }
    std::string_view rest = line.substr(4);
    const std::size_t spacePos = rest.find(' ');
    if (spacePos == std::string_view::npos) return false;
    mnemonic = std::string(rest.substr(0, spacePos));
    rest = rest.substr(spacePos + 1);

    const std::size_t comma1 = rest.find(',');
    if (comma1 == std::string_view::npos) return false;
    rd = trimReg(rest.substr(0, comma1));
    rest = rest.substr(comma1 + 1);

    const std::size_t comma2 = rest.find(',');
    if (comma2 == std::string_view::npos) return false;
    rs = trimReg(rest.substr(0, comma2));
    immStr = trimReg(rest.substr(comma2 + 1));

    return !mnemonic.empty() && !rd.empty() && !rs.empty() && !immStr.empty();
}

// Check if a line is a 3-register ALU instruction: mnemonic rd, rs1, rs2.
// Returns the mnemonic, rd, rs1, rs2 if so.
bool parseAlu3(std::string_view line,
               std::string& mnemonic,
               std::string& rd,
               std::string& rs1,
               std::string& rs2) {
    // Must start with 4 spaces (instruction, not label).
    if (line.size() < 5 || line.substr(0, 4) != "    ") {
        return false;
    }
    std::string_view rest = line.substr(4);

    // Extract mnemonic.
    const std::size_t spacePos = rest.find(' ');
    if (spacePos == std::string_view::npos) {
        return false;
    }
    mnemonic = std::string(rest.substr(0, spacePos));
    rest = rest.substr(spacePos + 1);

    // Parse "rd, rs1, rs2".
    const std::size_t comma1 = rest.find(',');
    if (comma1 == std::string_view::npos) return false;
    rd = trimReg(rest.substr(0, comma1));
    rest = rest.substr(comma1 + 1);

    const std::size_t comma2 = rest.find(',');
    if (comma2 == std::string_view::npos) return false;
    rs1 = trimReg(rest.substr(0, comma2));
    rs2 = trimReg(rest.substr(comma2 + 1));

    return !mnemonic.empty() && !rd.empty() && !rs1.empty() && !rs2.empty();
}

// Try to merge: ALUImm rd, rs, imm; mv rd2, rd → ALUImm rd2, rs, imm.
// RISC-V immediate instructions allow rd to overlap with rs.
bool tryMergeAluImmMv(const std::string& aluLine,
                      const std::string& mvLine,
                      std::string& merged) {
    std::string mnemonic, rd, rs, immStr;
    if (!parseAlu2Imm(aluLine, mnemonic, rd, rs, immStr)) {
        return false;
    }

    std::string mvDst, mvSrc;
    if (!parseMv(mvLine, mvDst, mvSrc)) {
        return false;
    }

    // mv source must be the ALU destination.
    if (mvSrc != rd) {
        return false;
    }

    // Reconstruct the original rs and imm from the aluLine.
    std::string_view rest = std::string_view(aluLine).substr(4);
    const std::size_t spacePos = rest.find(' ');
    rest = rest.substr(spacePos + 1);
    const std::size_t comma1 = rest.find(',');
    const std::size_t comma2 = rest.find(',', comma1 + 1);

    std::string origRs = std::string(rest.substr(comma1 + 1, comma2 - comma1 - 1));
    std::string origImm = std::string(rest.substr(comma2 + 1));

    // Trim trailing comment and whitespace from imm.
    const std::size_t commentPos = origImm.find('#');
    std::string comment;
    if (commentPos != std::string::npos) {
        comment = origImm.substr(commentPos);
        origImm = origImm.substr(0, commentPos);
    }
    while (!origImm.empty() && std::isspace(static_cast<unsigned char>(origImm.back()))) {
        origImm.pop_back();
    }

    merged = "    " + mnemonic + " " + mvDst + "," + origRs + "," + origImm;
    if (!comment.empty()) {
        merged += " " + comment;
    }
    return true;
}

// Try to merge: ALU rd, rs1, rs2; mv rd2, rd → ALU rd2, rs1, rs2.
// RISC-V ALU instructions allow rd to overlap with rs1 or rs2.
// For non-commutative ops (sub, div, rem, sll, srl, sra, slt), the source
// operand order is preserved — only the destination register changes.
// Returns true if the merge was performed (output has the merged instruction,
// and the mv is skipped).
bool tryMergeAluMv(const std::string& aluLine,
                   const std::string& mvLine,
                   std::string& merged) {
    std::string mnemonic, rd, rs1, rs2;
    if (!parseAlu3(aluLine, mnemonic, rd, rs1, rs2)) {
        return false;
    }

    std::string mvDst, mvSrc;
    if (!parseMv(mvLine, mvDst, mvSrc)) {
        return false;
    }

    // mv source must be the ALU destination.
    if (mvSrc != rd) {
        return false;
    }

    // mvDst can equal rs1 or rs2 — RISC-V allows rd to overlap with sources.
    // The instruction reads all sources before writing the destination.

    // Build the merged instruction: mnemonic rd2, rs1, rs2.
    // Reconstruct the original operand format preserving whitespace.
    std::string_view rest = std::string_view(aluLine).substr(4); // skip "    "
    const std::size_t spacePos = rest.find(' ');
    rest = rest.substr(spacePos + 1); // skip mnemonic

    const std::size_t comma1 = rest.find(',');
    const std::size_t comma2 = rest.find(',', comma1 + 1);

    std::string origRs1 = std::string(rest.substr(comma1 + 1, comma2 - comma1 - 1));
    std::string origRs2 = std::string(rest.substr(comma2 + 1));

    // Trim trailing comment from rs2.
    const std::size_t commentPos = origRs2.find('#');
    std::string comment;
    if (commentPos != std::string::npos) {
        comment = origRs2.substr(commentPos);
        origRs2 = origRs2.substr(0, commentPos);
    }

    // Trim trailing whitespace from rs2.
    while (!origRs2.empty() && std::isspace(static_cast<unsigned char>(origRs2.back()))) {
        origRs2.pop_back();
    }

    merged = "    " + mnemonic + " " + mvDst + "," + origRs1 + "," + origRs2;
    if (!comment.empty()) {
        merged += " " + comment;
    }
    return true;
}

// Try to collapse: mv t, a; mv dst, t → mv dst, a.
bool tryCollapseMvChain(const std::string& mv1Line,
                        const std::string& mv2Line,
                        std::string& collapsed) {
    std::string mv1Dst, mv1Src;
    if (!parseMv(mv1Line, mv1Dst, mv1Src)) {
        return false;
    }

    std::string mv2Dst, mv2Src;
    if (!parseMv(mv2Line, mv2Dst, mv2Src)) {
        return false;
    }

    // mv2's source must be mv1's destination.
    if (mv2Src != mv1Dst) {
        return false;
    }

    // Don't collapse if mv1Dst is also mv2Dst (would be a redundant move).
    if (mv1Dst == mv2Dst) {
        return false;
    }

    collapsed = "    mv " + mv2Dst + ", " + mv1Src;
    return true;
}

std::string definedRegister(std::string_view line) {
    const std::string text = stripInlineComment(line);
    if (!text.starts_with("    ")) {
        return {};
    }
    std::string_view rest(text);
    rest.remove_prefix(4);
    const std::size_t spacePos = rest.find(' ');
    const std::string mnemonic =
        spacePos == std::string_view::npos ? std::string(rest) : std::string(rest.substr(0, spacePos));
    if (mnemonic.empty() || mnemonic == "sw" || mnemonic == "j" || mnemonic == "ret" ||
        mnemonic == "call" || mnemonic.starts_with('b')) {
        return {};
    }
    if (spacePos == std::string_view::npos) {
        return {};
    }
    std::string_view operands = rest.substr(spacePos + 1);
    const std::size_t commaPos = operands.find(',');
    if (commaPos != std::string_view::npos) {
        operands = operands.substr(0, commaPos);
    }
    return trimReg(operands);
}

bool usesRegisterAsSource(std::string_view line, std::string_view reg) {
    std::string text = stripInlineComment(line);
    if (!text.starts_with("    ")) {
        return false;
    }

    std::string_view checkText(text);
    checkText.remove_prefix(4);
    const std::size_t spacePos = checkText.find(' ');
    if (spacePos == std::string_view::npos) {
        return false;
    }

    const std::string mnemonic(checkText.substr(0, spacePos));
    checkText = checkText.substr(spacePos + 1);
    if (mnemonic != "sw" && mnemonic != "j" && mnemonic != "ret" && mnemonic != "call" &&
        !mnemonic.starts_with('b')) {
        const std::size_t commaPos = checkText.find(',');
        if (commaPos == std::string_view::npos) {
            return false;
        }
        checkText = checkText.substr(commaPos + 1);
    }
    return registerTokenAppears(checkText, reg);
}

bool removedRegisterDeadAfter(const std::vector<std::string>& lines,
                              std::size_t start,
                              std::string_view reg) {
    for (std::size_t i = start; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        if (isBlankOrComment(line)) {
            continue;
        }
        if (stripInlineComment(line) == "    ret") {
            return true;
        }
        if (isLabelOrDirective(line) || isControlFlow(line)) {
            return false;
        }
        if (usesRegisterAsSource(line, reg)) {
            return false;
        }
        if (definedRegister(line) == reg) {
            return true;
        }
    }
    return true;
}

bool tryMergeAluMvIfDead(const std::vector<std::string>& lines,
                         std::size_t index,
                         std::string& merged) {
    std::string mnemonic;
    std::string rd;
    std::string rs;
    std::string imm;
    std::string rs1;
    std::string rs2;
    std::string mvDst;
    std::string mvSrc;

    if (!parseMv(lines[index + 1], mvDst, mvSrc)) {
        return false;
    }
    if (parseAlu3(lines[index], mnemonic, rd, rs1, rs2) && mvSrc == rd &&
        removedRegisterDeadAfter(lines, index + 2, rd)) {
        return tryMergeAluMv(lines[index], lines[index + 1], merged);
    }
    if (parseAlu2Imm(lines[index], mnemonic, rd, rs, imm) && mvSrc == rd &&
        removedRegisterDeadAfter(lines, index + 2, rd)) {
        return tryMergeAluImmMv(lines[index], lines[index + 1], merged);
    }
    return false;
}

bool tryCollapseMvChainIfDead(const std::vector<std::string>& lines,
                              std::size_t index,
                              std::string& collapsed) {
    std::string mv1Dst;
    std::string mv1Src;
    std::string mv2Dst;
    std::string mv2Src;
    if (!parseMv(lines[index], mv1Dst, mv1Src) ||
        !parseMv(lines[index + 1], mv2Dst, mv2Src) ||
        mv2Src != mv1Dst) {
        return false;
    }
    return removedRegisterDeadAfter(lines, index + 2, mv1Dst) &&
           tryCollapseMvChain(lines[index], lines[index + 1], collapsed);
}

bool parseLiZero(std::string_view line, std::string& destReg) {
    constexpr std::string_view prefix = "    li ";
    if (!line.starts_with(prefix)) {
        return false;
    }
    std::string_view rest = line.substr(prefix.size());
    const std::size_t commaPos = rest.find(',');
    if (commaPos == std::string_view::npos) {
        return false;
    }
    destReg = std::string(rest.substr(0, commaPos));
    std::string_view immText = rest.substr(commaPos + 1);
    while (!immText.empty() && std::isspace(static_cast<unsigned char>(immText.front()))) {
        immText.remove_prefix(1);
    }
    const std::size_t commentPos = immText.find('#');
    if (commentPos != std::string_view::npos) {
        immText = immText.substr(0, commentPos);
    }
    while (!immText.empty() && std::isspace(static_cast<unsigned char>(immText.back()))) {
        immText.remove_suffix(1);
    }
    return immText == "0";
}

bool parseSwFromReg(std::string_view line, std::string_view reg, std::string& slotOperand) {
    const std::string prefix = "    sw " + std::string(reg) + ", ";
    if (!line.starts_with(prefix)) {
        return false;
    }
    slotOperand = std::string(line.substr(prefix.size()));
    const std::size_t commentPos = slotOperand.find('#');
    if (commentPos != std::string::npos) {
        slotOperand = slotOperand.substr(0, commentPos);
    }
    while (!slotOperand.empty() && std::isspace(static_cast<unsigned char>(slotOperand.back()))) {
        slotOperand.pop_back();
    }
    return !slotOperand.empty();
}

bool tryFoldZeroStore(const std::string& liLine, const std::string& swLine, std::string& folded) {
    std::string destReg;
    if (!parseLiZero(liLine, destReg)) {
        return false;
    }
    std::string slotOperand;
    if (!parseSwFromReg(swLine, destReg, slotOperand)) {
        return false;
    }
    folded = "    sw zero, " + slotOperand;
    return true;
}

// Prune unused callee-saved register saves/restores.
// Removes "sw sN, ..." in prologue and "lw sN, ..." in epilogue for s-registers
// that are not used in the function body.
void pruneCalleeSaved(std::vector<std::string>& lines) {
    std::size_t i = 0;
    while (i < lines.size()) {
        // Find function start: a non-indented label that looks like a function name.
        // Function labels don't contain "__" (which is used for block labels like
        // main__while_cond_0) and don't start with "." (directives/epilogue labels).
        if (!isLabelOrDirective(lines[i]) || lines[i].starts_with(".")) {
            ++i;
            continue;
        }
        const std::string funcLabel = lines[i].substr(0, lines[i].size() - 1);
        if (funcLabel.starts_with(".") || funcLabel.find("__") != std::string::npos) {
            ++i;
            continue;
        }

        // Find the epilogue label (.L...epilogue).
        std::size_t epilogueIdx = lines.size();
        for (std::size_t j = i + 1; j < lines.size(); ++j) {
            if (lines[j].find("epilogue") != std::string::npos && lines[j].ends_with(":")) {
                epilogueIdx = j;
                break;
            }
            // Stop at next function (non-indented label that's not a .L or block label).
            if (j > i + 1 && isLabelOrDirective(lines[j]) &&
                !lines[j].starts_with("    ") && !lines[j].starts_with(".L") &&
                lines[j].find("__") == std::string::npos) {
                break;
            }
        }

        if (epilogueIdx >= lines.size()) {
            ++i;
            continue;
        }

        // Scan the ENTIRE function for s-reg usage, EXCLUDING the prologue sw
        // and epilogue lw instructions themselves. An s-register can be pruned
        // only if it never appears in ANY other instruction in the function.
        bool sUsed[11] = {};
        for (std::size_t j = i + 1; j < lines.size(); ++j) {
            // Stop at next function (non-indented, non-.L label that's not a block label).
            // Block labels contain "__" (e.g., main__while_cond_0).
            if (j > i + 1 && isLabelOrDirective(lines[j]) &&
                !lines[j].starts_with("    ") && !lines[j].starts_with(".L") &&
                lines[j].find("__") == std::string::npos) {
                break;
            }
            const auto& line = lines[j];
            if (isBlankOrComment(line) || isLabelOrDirective(line)) continue;

            // Skip prologue sw sN and epilogue lw sN — these are the instructions
            // we might prune, so don't count them as "usage".
            bool isCalleeSaveRestore = false;
            for (int s = 0; s < 11; ++s) {
                const std::string sReg = "s" + std::to_string(s + 1);
                const std::string swPrefix = "    sw " + sReg + ", ";
                const std::string lwPrefix = "    lw " + sReg + ", ";
                if (line.starts_with(swPrefix) || line.starts_with(lwPrefix)) {
                    isCalleeSaveRestore = true;
                    break;
                }
            }
            if (isCalleeSaveRestore) continue;

            // Check if any s-register appears in this line.
            for (int s = 0; s < 11; ++s) {
                const std::string sReg = "s" + std::to_string(s + 1);
                std::size_t pos = 0;
                while (true) {
                    pos = line.find(sReg, pos);
                    if (pos == std::string::npos) break;
                    const std::size_t after = pos + sReg.size();
                    if (after >= line.size() ||
                        line[after] == ',' || line[after] == ' ' ||
                        line[after] == '(' || line[after] == '#' || line[after] == '\t') {
                        if (pos == 0 || (!std::isalnum(static_cast<unsigned char>(line[pos - 1])) &&
                                         line[pos - 1] != '_')) {
                            sUsed[s] = true;
                            break;
                        }
                    }
                    pos += 1;
                }
            }
        }

        // Debug: print which s-registers are used.
        for (int s = 0; s < 11; ++s) {
            if (!sUsed[s]) {
                // This register will be pruned.
            }
        }

        // Remove prologue sw for s-registers that are NOT used in the body.
        for (std::size_t j = i + 1; j < epilogueIdx; ++j) {
            for (int s = 0; s < 11; ++s) {
                if (sUsed[s]) continue;
                const std::string sReg = "s" + std::to_string(s + 1);
                const std::string swPrefix = "    sw " + sReg + ", ";
                if (lines[j].starts_with(swPrefix)) {
                    lines[j] = "";
                }
            }
        }

        // Remove epilogue lw for s-registers that are NOT used in the body.
        for (std::size_t j = epilogueIdx; j < lines.size(); ++j) {
            if (j > epilogueIdx && isLabelOrDirective(lines[j]) &&
                !lines[j].starts_with("    ") && !lines[j].starts_with(".")) {
                break;
            }
            for (int s = 0; s < 11; ++s) {
                if (sUsed[s]) continue;
                const std::string sReg = "s" + std::to_string(s + 1);
                const std::string lwPrefix = "    lw " + sReg + ", ";
                if (lines[j].starts_with(lwPrefix)) {
                    lines[j] = "";
                }
            }
        }

        ++i;
    }

    // Remove marked lines.
    auto newEnd = std::remove_if(lines.begin(), lines.end(),
                                 [](const std::string& l) { return l.empty(); });
    lines.erase(newEnd, lines.end());
}

std::string joinLines(const std::vector<std::string>& lines) {
    std::ostringstream out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        out << lines[i];
        if (i + 1 < lines.size()) {
            out << '\n';
        }
    }
    if (!lines.empty() && !lines.back().empty()) {
        out << '\n';
    }
    return out.str();
}

} // namespace

std::string PeepholeOptimizer::optimize(std::string assembly) {
    assembly = machine::optimizeStackForwarding(std::move(assembly));
    std::vector<std::string> input = splitLines(assembly);
    // Debug: show the final output lines with addi.
    (void)input; // suppress unused warning

    // Pass 1: ALU + mv merging, mv chain collapsing, zero-store folding, redundant mv.
    std::vector<std::string> pass1;
    pass1.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        // Try zero-store folding: li reg, 0; sw reg, slot → sw zero, slot.
        if (i + 1 < input.size()) {
            std::string foldedZeroStore;
            if (tryFoldZeroStore(input[i], input[i + 1], foldedZeroStore)) {
                pass1.push_back(foldedZeroStore);
                ++i;
                continue;
            }
        }

        // Try ALU + mv merging: ALU rd, rs1, rs2; mv rd2, rd → ALU rd2, rs1, rs2.
        // Also: ALUImm rd, rs, imm; mv rd2, rd → ALUImm rd2, rs, imm.
        if (i + 1 < input.size()) {
            // Check that there's no label/control flow between the two instructions.
            if (!isLabelOrDirective(input[i]) && !isControlFlow(input[i]) &&
                !isLabelOrDirective(input[i + 1]) && !isControlFlow(input[i + 1])) {
                std::string merged;
                if (tryMergeAluMvIfDead(input, i, merged)) {
                    pass1.push_back(merged);
                    ++i; // Skip the mv.
                    continue;
                }
            }
        }

        // Try mv chain collapsing: mv t, a; mv dst, t → mv dst, a.
        if (i + 1 < input.size()) {
            if (!isLabelOrDirective(input[i]) && !isControlFlow(input[i]) &&
                !isLabelOrDirective(input[i + 1]) && !isControlFlow(input[i + 1])) {
                std::string collapsed;
                if (tryCollapseMvChainIfDead(input, i, collapsed)) {
                    pass1.push_back(collapsed);
                    ++i; // Skip the second mv.
                    continue;
                }
            }
        }

        // Remove redundant mv (mv reg, reg).
        if (isRedundantMove(input[i])) {
            continue;
        }

        // Remove fall-through jumps.
        std::string jumpTarget;
        if (parseUnconditionalJump(input[i], jumpTarget)) {
            std::size_t next = i + 1;
            while (next < input.size() && isBlankOrComment(input[next])) {
                ++next;
            }
            if (next < input.size() && isLabelLine(input[next], jumpTarget)) {
                continue;
            }
        }

        pass1.push_back(input[i]);
    }

    // Pass 1b: Second merge pass on pass1 output. This catches cases where a
    // merged instruction (e.g., addi s5, t0, 7 from merging addi+mv) is now
    // adjacent to another mv (e.g., mv s2, s5) that should also be merged.
    std::vector<std::string> pass1b;
    pass1b.reserve(pass1.size());
    for (std::size_t i = 0; i < pass1.size(); ++i) {
        if (i + 1 < pass1.size()) {
            if (!isLabelOrDirective(pass1[i]) && !isControlFlow(pass1[i]) &&
                !isLabelOrDirective(pass1[i + 1]) && !isControlFlow(pass1[i + 1])) {
                std::string merged;
                if (tryMergeAluMvIfDead(pass1, i, merged)) {
                    pass1b.push_back(merged);
                    ++i;
                    continue;
                }
                if (tryCollapseMvChainIfDead(pass1, i, merged)) {
                    pass1b.push_back(merged);
                    ++i;
                    continue;
                }
            }
        }
        pass1b.push_back(pass1[i]);
    }

    // The second merge pass can expose a self-copy after rewriting adjacent
    // instructions. Remove it before frame pruning and final emission.
    pass1b.erase(std::remove_if(pass1b.begin(), pass1b.end(), [](const std::string& line) {
                     return isRedundantMove(line);
                 }),
                 pass1b.end());

    // Pass 2: Callee-saved register pruning.
    pruneCalleeSaved(pass1b);

    return joinLines(pass1b);
}

} // namespace toyc::codegen
