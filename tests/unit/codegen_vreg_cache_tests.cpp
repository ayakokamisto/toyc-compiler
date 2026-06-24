#include "codegen/RiscvBackend.h"
#include "codegen/emit/RiscvEmitter.h"
#include "codegen/frame/StackFrame.h"
#include "codegen/frame/VRegAssignment.h"
#include "codegen/lower/InstructionSelector.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen vreg cache test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

std::size_t countSubstring(std::string_view text, std::string_view needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

toyc::codegen::contract::IRModule selfAddModule() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%a", 5},
                    toyc::codegen::contract::AddInst{"%b", "%a", "%a"},
                },
                toyc::codegen::contract::ReturnInst{"%b"},
            },
        },
    });
    return module;
}

void testOptReusesCachedVRegForDuplicateOperands() {
    const toyc::codegen::contract::IRModule module = selfAddModule();

    const std::string baseline = toyc::codegen::RiscvBackend().generate(module);
    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string optimized = toyc::codegen::RiscvBackend().generate(module, options);

    const std::size_t baselineLoads = countSubstring(baseline, "    lw ");
    const std::size_t optimizedLoads = countSubstring(optimized, "    lw ");
    require(optimizedLoads < baselineLoads,
            "-opt should emit fewer stack loads when both operands are the same vreg");
    require(optimized.find("    mv t1, t0\n") != std::string::npos ||
                optimized.find("    add s") != std::string::npos,
            "duplicate operand should reuse a cached register or direct physical register");
}

void testOptConstOneUsesAddi() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%one", 1},
                },
                toyc::codegen::contract::ReturnInst{"%one"},
            },
        },
    });

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly = toyc::codegen::RiscvBackend().generate(module, options);
    require(assembly.find("    addi ") != std::string::npos &&
                assembly.find(", zero, 1\n") != std::string::npos,
            "CONST 1 should use addi under -opt");
    require(assembly.find("    li t0, 1\n") == std::string::npos,
            "CONST 1 should not emit li under -opt");
}

void testCallInvalidatesBlockCache() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "callee",
        toyc::codegen::contract::Type::Void,
        {},
        {
            {
                "entry",
                {},
                toyc::codegen::contract::ReturnInst{std::nullopt},
            },
        },
    });
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%a", 3},
                    toyc::codegen::contract::CallVoidInst{"callee", {"%a"}},
                    toyc::codegen::contract::AddInst{"%b", "%a", "%a"},
                },
                toyc::codegen::contract::ReturnInst{"%b"},
            },
        },
    });

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly = toyc::codegen::RiscvBackend().generate(module, options);

    const std::size_t mainStart = assembly.find("main:\n");
    const std::size_t mainEnd = assembly.find("main__epilogue:");
    require(mainStart != std::string::npos && mainEnd != std::string::npos, "main body markers");
    const std::string mainBody = assembly.substr(mainStart, mainEnd - mainStart);

    const std::size_t loadsAfterCall = countSubstring(mainBody.substr(mainBody.find("call callee")),
                                                       "    lw ");
    const bool usesPhysicalReload =
        mainBody.find("    mv t0, s1\n") != std::string::npos ||
        mainBody.find("    mv t1, s1\n") != std::string::npos;
    const bool usesDirectPhysicalAdd = mainBody.find("    add s") != std::string::npos;
    require(loadsAfterCall == 1 || usesPhysicalReload || usesDirectPhysicalAdd,
            "after call, duplicate-operand add must reload or use a callee-saved register");
    require(mainBody.find("    mv t1, t0\n") != std::string::npos ||
                mainBody.find("    mv t1, s1\n") != std::string::npos || usesDirectPhysicalAdd,
            "second duplicate operand after call should reuse via mv or direct physical add");
}

void testOptCopyLoadsStackSourceDirectlyIntoPhysicalDestination() {
    toyc::codegen::StackFrame frame;
    frame.addVReg("%src");
    frame.finalize();

    toyc::codegen::VRegAssignment assignment;
    assignment.assignPhysical("%dst", "s1");

    toyc::codegen::RiscvEmitter emitter;
    toyc::codegen::InstructionSelector selector(emitter, frame, assignment, true);
    selector.emit(toyc::codegen::contract::CopyInst{"%dst", "%src"});

    const std::string assembly = emitter.str();
    require(assembly.find("    lw s1, -12(s0)\n") != std::string::npos,
            "COPY to physical destination should load stack source directly into that register");
    require(assembly.find("    lw t0, -12(s0)\n") == std::string::npos,
            "COPY to physical destination should not load through t0");
    require(assembly.find("    mv s1, t0\n") == std::string::npos,
            "COPY to physical destination should not move from t0 after loading");
}

void testOptPhysicalBinaryOpUsesAssignedRegistersDirectly() {
    toyc::codegen::StackFrame frame;
    frame.finalize();

    toyc::codegen::VRegAssignment assignment;
    assignment.assignPhysical("%lhs", "s1");
    assignment.assignPhysical("%rhs", "s2");
    assignment.assignPhysical("%dst", "s3");

    toyc::codegen::RiscvEmitter emitter;
    toyc::codegen::InstructionSelector selector(emitter, frame, assignment, true);
    selector.emit(toyc::codegen::contract::AddInst{"%dst", "%lhs", "%rhs"});

    const std::string assembly = emitter.str();
    require(assembly == "    add s3, s1, s2\n",
            "ADD with physical src/dst should emit a direct three-address instruction");
    require(assembly.find("    mv t0, s1\n") == std::string::npos,
            "direct physical ADD should not move lhs through t0");
    require(assembly.find("    mv s3, t0\n") == std::string::npos,
            "direct physical ADD should not move result back from t0");
}

} // namespace

int main() {
    try {
        testOptReusesCachedVRegForDuplicateOperands();
        testOptConstOneUsesAddi();
        testCallInvalidatesBlockCache();
        testOptCopyLoadsStackSourceDirectlyIntoPhysicalDestination();
        testOptPhysicalBinaryOpUsesAssignedRegistersDirectly();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
