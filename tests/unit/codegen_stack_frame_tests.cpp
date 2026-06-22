#include "codegen/RiscvEmitter.h"
#include "codegen/StackFrame.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen stack frame test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

void testEmitterFormatsAssembly() {
    toyc::codegen::RiscvEmitter emitter;
    emitter.section(".text");
    emitter.global("main");
    emitter.label("main");
    emitter.instruction("addi", {"sp", "sp", "-16"}, "allocate frame");
    emitter.instruction("ret");

    const std::string expected =
        "    .text\n"
        "    .global main\n"
        "main:\n"
        "    addi sp, sp, -16    # allocate frame\n"
        "    ret\n";
    require(emitter.str() == expected, "emitter output mismatch");
}

void testStackFrameAssignsAlignedSlots() {
    toyc::codegen::StackFrame frame;
    frame.addVReg("%x");
    frame.addVReg("%tmp");
    frame.addVReg("%x");
    frame.finalize();

    require(frame.frameSizeBytes() == 16, "two vregs plus ra/s0 should fit in 16 bytes");
    require(frame.vregSlots().size() == 2, "duplicate vreg should not allocate a slot");
    require(frame.vregOffsetFromS0("%x") == -12, "%x offset");
    require(frame.vregOffsetFromS0("%tmp") == -16, "%tmp offset");
    require(frame.savedRegisterSlots()[0].reg == "ra", "first saved register");
    require(frame.savedRegisterSlots()[0].offsetFromSp == 12, "ra offset from sp");
    require(frame.savedRegisterSlots()[1].reg == "s0", "second saved register");
    require(frame.savedRegisterSlots()[1].offsetFromSp == 8, "s0 offset from sp");
}

void testStackFrameIncludesCalleeSavedAndOutgoingArgs() {
    toyc::codegen::StackFrame frame;
    frame.addCalleeSavedRegister("s1");
    frame.addCalleeSavedRegister("s2");
    frame.addVReg("%value");
    frame.setOutgoingArgBytes(20);
    frame.finalize();

    require(frame.outgoingArgBytes() == 32, "outgoing arg area is 16-byte aligned");
    require(frame.frameSizeBytes() == 32, "fixed frame excludes per-call outgoing arg area");
    require(frame.savedRegisterSlots().size() == 4, "ra/s0 plus two saved registers");
    require(frame.savedRegisterSlots()[2].reg == "s1", "s1 saved slot");
    require(frame.savedRegisterSlots()[2].offsetFromS0 == -12, "s1 offset from s0");
    require(frame.vregOffsetFromS0("%value") == -20, "%value after saved registers");
}

} // namespace

int main() {
    try {
        testEmitterFormatsAssembly();
        testStackFrameAssignsAlignedSlots();
        testStackFrameIncludesCalleeSavedAndOutgoingArgs();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
