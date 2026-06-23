#include "codegen/abi/CallingConvention.h"
#include "codegen/emit/RiscvEmitter.h"
#include "codegen/frame/StackFrame.h"
#include "codegen/frame/VRegAssignment.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen calling convention test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

void requireContains(const std::string& text, std::string_view fragment, std::string_view message) {
    if (text.find(fragment) == std::string::npos) {
        std::cerr << "missing fragment in output:\n" << text << "\nfragment: \"" << fragment
                  << "\"\n";
        fail(message);
    }
}

void testStackArgBytesFor() {
    require(toyc::codegen::CallingConvention::stackArgBytesFor(0) == 0,
            "zero arguments need no stack area");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(8) == 0,
            "eight register arguments need no stack area");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(9) == 16,
            "ninth argument requires 16-byte aligned stack area");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(10) == 16,
            "tenth argument stays within first aligned stack chunk");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(11) == 16,
            "eleventh argument stays within first aligned stack chunk");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(12) == 16,
            "twelfth argument stays within first aligned stack chunk");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(13) == 32,
            "thirteenth argument requires next aligned stack chunk");
}

void testEmitParamLandingUsesAbiRegisters() {
    toyc::codegen::StackFrame frame;
    frame.addVReg("%p0");
    frame.finalize();

    toyc::codegen::contract::IRFunction function;
    function.name = "f";
    function.returnType = toyc::codegen::contract::Type::Int;
    function.params = {{"x", "%p0"}};

    toyc::codegen::RiscvEmitter emitter;
    toyc::codegen::CallingConvention abi(emitter, frame);
    abi.emitParamLanding(function);

    const std::string assembly = emitter.str();
    requireContains(assembly, "    sw a0, -12(s0)\n", "first parameter lands from a0 to stack slot");
}

void testEmitParamLandingSupportsPhysicalRegisters() {
    toyc::codegen::StackFrame frame;
    for (int i = 1; i < 8; ++i) {
        frame.addVReg("%unused" + std::to_string(i));
    }
    frame.finalize();

    toyc::codegen::VRegAssignment assignment;
    assignment.assignPhysical("%p0", "s1");
    assignment.assignPhysical("%p8", "s2");

    toyc::codegen::contract::IRFunction function;
    function.name = "f";
    function.returnType = toyc::codegen::contract::Type::Int;
    function.params = {
        {"p0", "%p0"},
        {"p1", "%unused1"},
        {"p2", "%unused2"},
        {"p3", "%unused3"},
        {"p4", "%unused4"},
        {"p5", "%unused5"},
        {"p6", "%unused6"},
        {"p7", "%unused7"},
        {"p8", "%p8"},
    };

    toyc::codegen::RiscvEmitter emitter;
    toyc::codegen::CallingConvention abi(emitter, frame, assignment);
    abi.emitParamLanding(function);

    const std::string assembly = emitter.str();
    requireContains(assembly, "    mv s1, a0\n",
                    "register-allocated first parameter lands in physical register");
    requireContains(assembly, "    lw t0, 0(s0)\n",
                    "ninth parameter is still read from incoming stack area");
    requireContains(assembly, "    mv s2, t0\n",
                    "register-allocated ninth parameter lands in physical register");
}

void testEmitCallArgsSpillsNinthArgumentToStack() {
    toyc::codegen::StackFrame frame;
    for (int i = 0; i < 9; ++i) {
        frame.addVReg("%a" + std::to_string(i));
    }
    frame.finalize();

    std::vector<std::string> args;
    args.reserve(9);
    for (int i = 0; i < 9; ++i) {
        args.push_back("%a" + std::to_string(i));
    }

    toyc::codegen::RiscvEmitter emitter;
    toyc::codegen::CallingConvention abi(emitter, frame);
    abi.emitCallArgs(args);

    const std::string assembly = emitter.str();
    requireContains(assembly, "    addi sp, sp, -16\n", "ninth argument reserves aligned stack area");
    requireContains(assembly, "    sw t0, 0(sp)\n", "ninth argument is stored on stack");
    requireContains(assembly, "    lw a0, -12(s0)\n", "first argument is loaded into a0");
}

void testEmitCallArgsSupportsPhysicalRegisters() {
    toyc::codegen::StackFrame frame;
    frame.finalize();

    toyc::codegen::VRegAssignment assignment;
    for (int i = 0; i < 9; ++i) {
        assignment.assignPhysical("%a" + std::to_string(i), "s" + std::to_string(i + 1));
    }

    std::vector<std::string> args;
    args.reserve(9);
    for (int i = 0; i < 9; ++i) {
        args.push_back("%a" + std::to_string(i));
    }

    toyc::codegen::RiscvEmitter emitter;
    toyc::codegen::CallingConvention abi(emitter, frame, assignment);
    abi.emitCallArgs(args);

    const std::string assembly = emitter.str();
    requireContains(assembly, "    addi sp, sp, -16\n",
                    "physical ninth argument still reserves aligned stack area");
    requireContains(assembly, "    mv a0, s1\n",
                    "first physical argument moves into ABI argument register");
    requireContains(assembly, "    mv t0, s9\n",
                    "ninth physical argument moves through temporary register");
    requireContains(assembly, "    sw t0, 0(sp)\n",
                    "ninth physical argument is written to outgoing stack area");
}

void testStoreVRegSupportsPhysicalReturnDestination() {
    toyc::codegen::StackFrame frame;
    frame.finalize();

    toyc::codegen::VRegAssignment assignment;
    assignment.assignPhysical("%ret", "s1");

    toyc::codegen::RiscvEmitter emitter;
    toyc::codegen::CallingConvention abi(emitter, frame, assignment);
    abi.storeVReg("%ret", "a0");

    const std::string assembly = emitter.str();
    requireContains(assembly, "    mv s1, a0\n",
                    "call return value can be stored into physical destination register");
}

} // namespace

int main() {
    try {
        testStackArgBytesFor();
        testEmitParamLandingUsesAbiRegisters();
        testEmitParamLandingSupportsPhysicalRegisters();
        testEmitCallArgsSpillsNinthArgumentToStack();
        testEmitCallArgsSupportsPhysicalRegisters();
        testStoreVRegSupportsPhysicalReturnDestination();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
