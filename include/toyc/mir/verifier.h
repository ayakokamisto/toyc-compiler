#pragma once

#include "toyc/mir/mir.h"

namespace toyc {

MIRVerificationResult verifyMIRFunction(const MIRFunction& function);
MIRVerificationResult verifyMIR(const MIRModule& module);

} // namespace toyc
