#pragma once

#include "codegen/ContractIR.h"
#include "codegen/RiscvBackend.h"

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
