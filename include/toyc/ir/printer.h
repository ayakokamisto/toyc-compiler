#pragma once
/// IR Printer — outputs IR in a human-readable text format.

#include <ostream>

namespace toyc {

class Module;

/// Dump an IR module to an output stream in stable, deterministic format.
void dumpIR(const Module& module, std::ostream& output);

} // namespace toyc
