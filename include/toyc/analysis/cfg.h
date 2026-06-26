#pragma once
/// Control Flow Graph utilities.
/// Rebuilds predecessor/successor edges from terminators.

namespace toyc {

class Function;
class Module;

/// Rebuild CFG edges (predecessors/successors) for a single function.
/// Clears existing edges and reconstructs from terminators.
void rebuildCFG(Function& func);

/// Rebuild CFG for all functions in a module.
void rebuildCFG(Module& module);

} // namespace toyc
