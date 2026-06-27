#pragma once
/// IR Verifier — checks structural well-formedness of the IR module.

#include <string>
#include <vector>

namespace toyc {

class Module;
class Function;

enum class VerificationMode {
  CanonicalSlot,
  SSA,
  Auto,
};

/// Result of IR verification.
struct VerificationResult {
  bool ok = true;
  std::vector<std::string> errors;

  void addError(std::string msg) {
    ok = false;
    errors.push_back(std::move(msg));
  }
};

/// Verify the entire IR module.
VerificationResult verifyModule(const Module& module,
                                VerificationMode mode = VerificationMode::Auto);

/// Verify a single function.
VerificationResult verifyFunction(const Function& func, const Module& module,
                                  VerificationMode mode = VerificationMode::Auto);

VerificationResult verifySSAFunction(const Function& func, const Module& module);
VerificationResult verifySSAModule(const Module& module);

} // namespace toyc
