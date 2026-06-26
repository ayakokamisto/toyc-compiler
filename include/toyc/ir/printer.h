#pragma once
/// IR Printer — outputs IR in a human-readable text format.
/// This is a P0 placeholder.

#include <ostream>

namespace toyc {

class Module;

/// Print an IR module to an output stream.
void printModule(const Module& module, std::ostream& out);

} // namespace toyc
