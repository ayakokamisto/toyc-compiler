#pragma once

#include "module.h"

#include <string>

// =============================================================================
// IRPrinter — textual IR pretty-printer.
//
// Produces stable output (deterministic iteration order) suitable for
// human reading, debugging, and diff-based testing.
// =============================================================================

class IRPrinter {
public:
    // Print the full program (module + all functions).
    static std::string print(const IRProgram& program);

    // Print a single module.
    static std::string print(const Module& module);

    // Print a single function.
    static std::string print(const Function& function);

    // Print a single basic block.
    static std::string print(const BasicBlock& block);

    // Print a single instruction (no trailing newline).
    static std::string print(const Instr& instr);

    // Print a single value.
    static std::string print(const Value& value);
};
