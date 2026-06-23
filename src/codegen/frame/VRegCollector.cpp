#include "codegen/frame/VRegCollector.h"

#include "codegen/frame/VRegAnalysis.h"

namespace toyc::codegen {

void VRegCollector::collectInto(const contract::IRFunction& function, StackFrame& frame) {
    const VRegAnalysis analysis = analyzeVRegs(function);
    for (const std::string& vreg : analysis.discoveryOrder) {
        frame.addVReg(vreg);
    }
    frame.setOutgoingArgBytes(analysis.maxOutgoingArgBytes);
}

} // namespace toyc::codegen
