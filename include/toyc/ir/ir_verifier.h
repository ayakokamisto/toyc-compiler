#pragma once

#include "toyc/ir/function.h"

#include <string>
#include <vector>

// =============================================================================
// IR verification — two layers:
//   verifyIR — structural correctness (all block terminators, labels, operands)
//   verifyP1EmitterSupport — P1 backend capability constraints
// =============================================================================

struct IRVerifierError {
    int line;       // For diagnostic reporting (0 if unknown)
    std::string message;
};

std::vector<IRVerifierError> verifyIR(const Function& fn);
std::vector<IRVerifierError> verifyP1EmitterSupport(const Function& fn);

// Returns true if no errors.
inline bool is_valid(const std::vector<IRVerifierError>& errors) {
    return errors.empty();
}
