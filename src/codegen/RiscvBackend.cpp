#include "codegen/RiscvBackend.h"

#include "codegen/CodegenUtils.h"
#include "codegen/FunctionEmitter.h"
#include "codegen/RiscvEmitter.h"

namespace toyc::codegen {

std::string RiscvBackend::generate(const contract::IRModule& module,
                                   const BackendOptions& options) {
    RiscvEmitter emitter;
    emitter.section(".data");
    for (const contract::GlobalObject& global : module.globalVars) {
        emitter.label(globalLabel(global.name));
        emitter.directive(".word", imm(global.initValue));
    }

    emitter.blankLine();
    emitter.section(".text");
    emitter.blankLine();

    FunctionEmitter functionEmitter(emitter, options);
    for (std::size_t i = 0; i < module.functions.size(); ++i) {
        functionEmitter.emit(module.functions[i]);
        if (i + 1 < module.functions.size()) {
            emitter.blankLine();
        }
    }

    (void)options.enableOpt;
    return emitter.str();
}

} // namespace toyc::codegen
