#pragma once
/// IR Verifier — checks structural well-formedness of the IR module.

#include <string>
#include <vector>

namespace toyc {

class Module;
class Function;

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
VerificationResult verifyModule(const Module& module);

/// Verify a single function.
VerificationResult verifyFunction(const Function& func, const Module& module);

} // namespace toyc
