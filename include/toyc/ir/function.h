#pragma once

#include "basic_block.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// =============================================================================
// Function — a single function definition.
//
// Owns:
//   - All BasicBlocks (unique_ptr)
//   - All function-scoped Values (Temp, LocalVar, Label, Constant)
//     via the owned_values_ arena.
//
// Provides factory methods (new_temp, new_local, etc.) that allocate a value
// in the arena and return a raw pointer to it. The pointer remains valid for
// the lifetime of this Function.
// =============================================================================

class Function {
public:
    // entry_label_hint is used to generate the entry block's label name
    // (e.g. "main.entry" → "main.entry.N").
    Function(std::string name, Type return_type, std::vector<LocalVar*> params,
             std::string entry_label_hint = "entry");

    const std::string& name() const { return name_; }
    Type return_type() const { return return_type_; }
    const std::vector<LocalVar*>& parameters() const { return params_; }
    void set_parameters(const std::vector<LocalVar*>& p) { params_ = p; }

    // --- Blocks ---
    BasicBlock* entry_block() const { return entry_block_; }
    const std::vector<std::unique_ptr<BasicBlock>>& blocks() const { return blocks_; }
    void add_block(std::unique_ptr<BasicBlock> block);

    // --- Locals ---
    const std::unordered_map<std::string, LocalVar*>& locals() const { return local_map_; }
    void add_local(LocalVar* local);

    // --- Value arena ---
    // These allocate a new Value in the arena and return a raw pointer.
    // The arena owns the value's lifetime.

    Temp* new_temp(Type type);
    LocalVar* new_local(const std::string& source_name, bool is_parameter);
    Label* new_label(const std::string& hint);
    Constant* new_constant(int32_t value);

    // --- Temp index tracking ---
    uint32_t next_temp_index() const { return next_temp_id_; }
    void set_next_temp_index(uint32_t n) { next_temp_id_ = n; }

private:
    struct ValueDeleter {
        void operator()(Value* v) { delete v; }
    };

    std::string name_;
    Type return_type_;
    std::vector<LocalVar*> params_;
    std::vector<std::unique_ptr<BasicBlock>> blocks_;
    BasicBlock* entry_block_;
    std::unordered_map<std::string, LocalVar*> local_map_;

    // Value arena: owns all Temp, LocalVar, Label, Constant values.
    std::vector<std::unique_ptr<Value>> owned_values_;

    // Counters for generating display names.
    uint32_t next_temp_id_ = 0;
    uint32_t next_local_id_ = 0;
    uint32_t next_label_id_ = 0;
    uint32_t next_const_id_ = 0;
};
