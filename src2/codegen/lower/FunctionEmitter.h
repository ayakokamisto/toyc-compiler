#pragma once

#include "codegen/BackendOptions.h"
#include "codegen/ContractIR.h"

namespace toyc::codegen {

class RiscvEmitter;

class FunctionEmitter {
public:
    FunctionEmitter(RiscvEmitter& emitter, const BackendOptions& options);

    void emit(const contract::IRFunction& function);

private:
    RiscvEmitter& emitter_;
    BackendOptions options_;
};

} // namespace toyc::codegen
