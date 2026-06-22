#include "codegen/frame/VRegCollector.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen vreg collector test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

toyc::codegen::contract::IRFunction makeFunctionWithCall(std::size_t argCount) {
    std::vector<std::string> args;
    args.reserve(argCount);
    for (std::size_t i = 0; i < argCount; ++i) {
        args.push_back("%a" + std::to_string(i));
    }

    return {
        "caller",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::CallInst{"%ret", "callee", args},
            },
            toyc::codegen::contract::ReturnInst{"%ret"},
        }},
    };
}

void testCollectsParamsAndInstructionOperands() {
    toyc::codegen::StackFrame frame;
    toyc::codegen::contract::IRFunction function = {
        "f",
        toyc::codegen::contract::Type::Int,
        {{"a", "%p0"}, {"b", "%p1"}},
        {{
            "entry",
            {
                toyc::codegen::contract::ConstInst{"%x", 1},
                toyc::codegen::contract::AddInst{"%sum", "%p0", "%x"},
                toyc::codegen::contract::CopyInst{"%y", "%sum"},
            },
            toyc::codegen::contract::BranchInst{"%y", "then_0", "else_0"},
        }},
    };

    toyc::codegen::VRegCollector::collectInto(function, frame);
    frame.finalize();

    require(frame.containsVReg("%p0"), "param vreg");
    require(frame.containsVReg("%p1"), "second param vreg");
    require(frame.containsVReg("%x"), "const dst vreg");
    require(frame.containsVReg("%sum"), "binary dst vreg");
    require(frame.containsVReg("%y"), "copy dst and branch cond vreg");
    require(frame.vregSlots().size() == 5, "deduplicated vreg slot count");
}

void testCollectsTerminatorOperands() {
    toyc::codegen::StackFrame frame;
    toyc::codegen::contract::IRFunction function = {
        "f",
        toyc::codegen::contract::Type::Void,
        {},
        {{
            "entry",
            {},
            toyc::codegen::contract::ReturnInst{},
        }},
    };

    toyc::codegen::VRegCollector::collectInto(function, frame);
    frame.finalize();
    require(frame.vregSlots().empty(), "void return without operands needs no vreg slots");
}

void testRecordsMaxOutgoingStackArgArea() {
    toyc::codegen::StackFrame smallCallFrame;
    toyc::codegen::VRegCollector::collectInto(makeFunctionWithCall(2), smallCallFrame);
    smallCallFrame.finalize();
    require(smallCallFrame.outgoingArgBytes() == 0, "two-arg call needs no stack arg area");

    toyc::codegen::StackFrame largeCallFrame;
    toyc::codegen::VRegCollector::collectInto(makeFunctionWithCall(10), largeCallFrame);
    largeCallFrame.finalize();
    require(largeCallFrame.outgoingArgBytes() == 16,
            "ten-arg call reserves one aligned stack chunk");

    toyc::codegen::StackFrame mixedCallFrame;
    toyc::codegen::contract::IRFunction function = {
        "caller",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::CallInst{"%a", "f", {"%x", "%y"}},
                toyc::codegen::contract::CallInst{
                    "%b",
                    "g",
                    {"%a0", "%a1", "%a2", "%a3", "%a4", "%a5", "%a6", "%a7", "%a8"},
                },
            },
            toyc::codegen::contract::ReturnInst{"%b"},
        }},
    };
    toyc::codegen::VRegCollector::collectInto(function, mixedCallFrame);
    mixedCallFrame.finalize();
    require(mixedCallFrame.outgoingArgBytes() == 16,
            "max outgoing stack arg area comes from the largest call site");
    require(mixedCallFrame.containsVReg("%a8"), "stack argument vreg is collected");
}

void testCollectsGlobalAndCompareOperands() {
    toyc::codegen::StackFrame frame;
    toyc::codegen::contract::IRFunction function = {
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::LoadGlobalInst{"%loaded", "@counter"},
                toyc::codegen::contract::StoreGlobalInst{"@counter", "%loaded"},
                toyc::codegen::contract::LtInst{"%cond", "%loaded", "%loaded"},
                toyc::codegen::contract::LNotInst{"%not", "%cond"},
            },
            toyc::codegen::contract::ReturnInst{"%not"},
        }},
    };

    toyc::codegen::VRegCollector::collectInto(function, frame);
    frame.finalize();
    require(frame.containsVReg("%loaded"), "global load/store vreg");
    require(frame.containsVReg("%cond"), "compare dst vreg");
    require(frame.containsVReg("%not"), "lnot dst vreg");
}

} // namespace

int main() {
    try {
        testCollectsParamsAndInstructionOperands();
        testCollectsTerminatorOperands();
        testRecordsMaxOutgoingStackArgArea();
        testCollectsGlobalAndCompareOperands();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
