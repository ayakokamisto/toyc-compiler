#include "toyc/ir/function.h"

Function::Function(std::string name, Type return_type,
                   std::vector<LocalVar*> params, std::string entry_label_hint)
    : name_(std::move(name)),
      return_type_(return_type),
      params_(std::move(params)) {

    // Create entry label and entry block.
    auto* entry_label = new_label(entry_label_hint);
    auto entry = std::make_unique<BasicBlock>(entry_label);
    entry_block_ = entry.get();
    blocks_.push_back(std::move(entry));

    for (auto* param : params_) {
        local_map_[param->name()] = param;
    }
}

void Function::add_block(std::unique_ptr<BasicBlock> block) {
    blocks_.push_back(std::move(block));
}

void Function::add_local(LocalVar* local) {
    local_map_[local->name()] = local;
}

// --- Value arena factory methods ---

Temp* Function::new_temp(Type type) {
    auto t = std::make_unique<Temp>(next_temp_id_++, type);
    auto* raw = t.get();
    owned_values_.push_back(std::move(t));
    return raw;
}

LocalVar* Function::new_local(const std::string& source_name, bool is_parameter) {
    auto l = std::make_unique<LocalVar>(next_local_id_++, source_name, is_parameter);
    auto* raw = l.get();
    add_local(raw);
    owned_values_.push_back(std::move(l));
    return raw;
}

Label* Function::new_label(const std::string& hint) {
    auto lbl = std::make_unique<Label>(hint + "." + std::to_string(next_label_id_++));
    auto* raw = lbl.get();
    owned_values_.push_back(std::move(lbl));
    return raw;
}

Constant* Function::new_constant(int32_t value) {
    auto c = std::unique_ptr<Constant>(Constant::of(next_const_id_++, value));
    auto* raw = c.get();
    owned_values_.push_back(std::move(c));
    return raw;
}
