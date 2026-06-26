#pragma once
/// IR Verifier — checks structural well-formedness of the IR.
/// This is a P0 placeholder.

#include <string>

namespace toyc {

class Module;

/// Verify the IR module. Returns empty string on success, error message on failure.
/// P0 stub: always succeeds.
std::string verifyModule(const Module& module);

} // namespace toyc
