#pragma once

#include "codegen/ContractIR.h"
#include "common/diagnostic.h"

#include <vector>

namespace toyc::ast {
struct CompUnit;
}

namespace toyc::sema {
class SemanticModel;
}

namespace toyc::ir {

class ContractIRGenerator {
public:
    [[nodiscard]] codegen::contract::IRModule
    generate(const ast::CompUnit& unit, const sema::SemanticModel& semanticModel);

    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept;

private:
    std::vector<Diagnostic> diagnostics_;
};

[[nodiscard]] bool verifyContractModule(const codegen::contract::IRModule& module,
                                        std::vector<Diagnostic>& diagnostics);

} // namespace toyc::ir
