#include "codegen/opt/PeepholeOptimizer.h"

#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace toyc::codegen {

namespace {

std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::istringstream stream{std::string(text)};
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
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
    const std::vector<std::string> input = splitLines(assembly);
    std::vector<std::string> output;
    output.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        if (i + 1 < input.size()) {
            std::string foldedZeroStore;
            if (tryFoldZeroStore(input[i], input[i + 1], foldedZeroStore)) {
                output.push_back(foldedZeroStore);
                ++i;
                continue;
            }
        }

        if (isRedundantMove(input[i])) {
            continue;
        }

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

        output.push_back(input[i]);
    }

    return joinLines(output);
}

} // namespace toyc::codegen
