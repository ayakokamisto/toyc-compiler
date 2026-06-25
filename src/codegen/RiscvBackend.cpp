#include "codegen/RiscvBackend.h"

#include "codegen/emit/CodegenUtils.h"
#include "codegen/emit/RiscvEmitter.h"
#include "codegen/lower/FunctionEmitter.h"
#include "codegen/opt/IrOptimizer.h"
#include "codegen/opt/PeepholeOptimizer.h"

#include <utility>

namespace toyc::codegen {

std::string RiscvBackend::generate(const contract::IRModule& module,
                                   const BackendOptions& options) {
    contract::IRModule workingModule = module;
    if (options.enableOpt) {
        IrOptimizer::optimize(workingModule);
    }

    RiscvEmitter emitter;
    emitter.section(".data");
    for (const contract::GlobalObject& global : workingModule.globalVars) {
        emitter.label(globalLabel(global.name));
        emitter.directive(".word", imm(global.initValue));
    }

    emitter.blankLine();
    emitter.section(".text");
    emitter.blankLine();

    FunctionEmitter functionEmitter(emitter, options);
    for (std::size_t i = 0; i < workingModule.functions.size(); ++i) {
        functionEmitter.emit(workingModule.functions[i]);
        if (i + 1 < workingModule.functions.size()) {
            emitter.blankLine();
        }
    }

    std::string assembly = emitter.str();
    if (options.enableOpt) {
        assembly = PeepholeOptimizer::optimize(std::move(assembly));
    }
    return assembly;
}

} // namespace toyc::codegen
