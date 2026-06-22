#include "codegen/frame/RegisterAllocator.h"

#include "codegen/frame/VRegCollector.h"

namespace toyc::codegen {

StackFrame RegisterAllocator::allocate(const contract::IRFunction& function) {
    StackFrame frame;
    VRegCollector::collectInto(function, frame);
    frame.finalize();
    return frame;
}

} // namespace toyc::codegen
