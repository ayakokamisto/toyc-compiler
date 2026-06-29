#include "codegen/RiscvBackend.h"

#include "codegen/emit/CodegenUtils.h"
#include "codegen/emit/RiscvEmitter.h"
#include "codegen/lower/FunctionEmitter.h"
#include "codegen/opt/PeepholeOptimizer.h"

#include <utility>

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

    std::string assembly = emitter.str();
    if (options.enableOpt) {
        assembly = PeepholeOptimizer::optimize(std::move(assembly));
    }
    std::string result;
    result.reserve(assembly.size());
    for (const char ch : assembly) {
        if (ch != '\r') {
            result.push_back(ch);
        }
    }
    return result;
}

} // namespace toyc::codegen
