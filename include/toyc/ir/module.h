#pragma once

#include "function.h"

#include <memory>
#include <vector>

// =============================================================================
// Module — top-level IR unit (compilation unit).
//
// Owns:
//   - All Function objects (unique_ptr)
//   - Global variables and global constants (unique_ptr<Value> arena)
//   - Global constants (Constant*, subset of owned_values_)
// =============================================================================

class Module {
public:
    // --- Functions ---
    const std::vector<std::unique_ptr<Function>>& functions() const { return functions_; }
    Function* main_function() const { return main_; }
    void add_function(std::unique_ptr<Function> fn);

    // --- Globals ---
    const std::vector<GlobalVar*>& globals() const { return globals_; }
    void add_global(GlobalVar* gv);

    const std::vector<Constant*>& global_constants() const { return global_constants_; }
    void add_global_constant(Constant* c);

    // --- Module-level arena ---
    // For GlobalVar and module-level Constant allocation.
    GlobalVar* new_global_var(const std::string& name, int32_t initial_value);
    Constant* new_global_constant(const std::string& name, int32_t value);

    // Unnamed constant factory.
    Constant* new_constant(int32_t value);

private:
    std::vector<std::unique_ptr<Function>> functions_;
    Function* main_ = nullptr;

    std::vector<GlobalVar*> globals_;
    std::vector<Constant*> global_constants_;

    // Arena for module-level Values (GlobalVar, unnamed Constant).
    std::vector<std::unique_ptr<Value>> owned_values_;

    uint32_t next_global_const_id_ = 0;
};

// Convenience: top-level program wrapper.
class IRProgram {
public:
    explicit IRProgram(std::unique_ptr<Module> mod)
        : module_(std::move(mod)) {}

    Module* module() const { return module_.get(); }

private:
    std::unique_ptr<Module> module_;
};
