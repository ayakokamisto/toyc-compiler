#include "toyc/ir/module.h"

void Module::add_function(std::unique_ptr<Function> fn) {
    if (fn->name() == "main") {
        main_ = fn.get();
    }
    functions_.push_back(std::move(fn));
}

void Module::add_global(GlobalVar* gv) {
    globals_.push_back(gv);
}

void Module::add_global_constant(Constant* c) {
    global_constants_.push_back(c);
}

GlobalVar* Module::new_global_var(const std::string& name, int32_t initial_value) {
    auto gv = std::make_unique<GlobalVar>(name, initial_value);
    auto* raw = gv.get();
    owned_values_.push_back(std::move(gv));
    globals_.push_back(raw);
    return raw;
}

Constant* Module::new_global_constant(const std::string& name, int32_t value) {
    auto c = std::unique_ptr<Constant>(Constant::named(next_global_const_id_++, name, value));
    auto* raw = c.get();
    owned_values_.push_back(std::move(c));
    global_constants_.push_back(raw);
    return raw;
}

Constant* Module::new_constant(int32_t value) {
    auto c = std::unique_ptr<Constant>(Constant::of(next_global_const_id_++, value));
    auto* raw = c.get();
    owned_values_.push_back(std::move(c));
    return raw;
}
