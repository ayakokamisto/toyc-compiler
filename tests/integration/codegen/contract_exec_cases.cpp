#include "codegen/RiscvBackend.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace toyc::codegen;
using namespace toyc::codegen::contract;

IRFunction makeSum10Function() {
    std::vector<Param> params;
    params.reserve(10);
    for (int i = 0; i < 10; ++i) {
        params.push_back({"p" + std::to_string(i), "%p" + std::to_string(i)});
    }

    std::vector<Instruction> instructions;
    instructions.push_back(AddInst{"%acc", "%p0", "%p1"});
    for (int i = 2; i < 10; ++i) {
        instructions.push_back(AddInst{"%acc", "%acc", "%p" + std::to_string(i)});
    }

    return {
        "sum10",
        Type::Int,
        params,
        {{
            "entry",
            instructions,
            ReturnInst{"%acc"},
        }},
    };
}

IRModule basicReturnModule() {
    IRModule module;
    module.functions.push_back({
        "main",
        Type::Int,
        {},
        {{
            "entry",
            {ConstInst{"%ret", 42}},
            ReturnInst{"%ret"},
        }},
    });
    return module;
}

IRModule loopModule() {
    IRModule module;
    module.functions.push_back({
        "main",
        Type::Int,
        {},
        {
            {
                "entry",
                {
                    ConstInst{"%i", 0},
                    ConstInst{"%sum", 0},
                    ConstInst{"%limit", 4},
                },
                JumpInst{"while_cond_0"},
            },
            {
                "while_cond_0",
                {LtInst{"%cond", "%i", "%limit"}},
                BranchInst{"%cond", "while_body_0", "while_exit_0"},
            },
            {
                "while_body_0",
                {
                    AddInst{"%sum", "%sum", "%i"},
                    ConstInst{"%one", 1},
                    AddInst{"%i", "%i", "%one"},
                },
                JumpInst{"while_cond_0"},
            },
            {
                "while_exit_0",
                {},
                ReturnInst{"%sum"},
            },
        },
    });
    return module;
}

IRModule manyParamsModule() {
    IRModule module;
    module.functions.push_back(makeSum10Function());

    std::vector<Instruction> instructions;
    std::vector<std::string> args;
    instructions.reserve(11);
    args.reserve(10);
    for (int i = 0; i < 10; ++i) {
        const std::string vreg = "%a" + std::to_string(i);
        instructions.push_back(ConstInst{vreg, i + 1});
        args.push_back(vreg);
    }
    instructions.push_back(CallInst{"%ret", "sum10", args});

    module.functions.push_back({
        "main",
        Type::Int,
        {},
        {{
            "entry",
            instructions,
            ReturnInst{"%ret"},
        }},
    });
    return module;
}

IRModule globalVarModule() {
    IRModule module;
    module.globalVars.push_back({"@g", 7});
    module.functions.push_back({
        "main",
        Type::Int,
        {},
        {{
            "entry",
            {
                LoadGlobalInst{"%x", "@g"},
                ConstInst{"%delta", 5},
                AddInst{"%updated", "%x", "%delta"},
                StoreGlobalInst{"@g", "%updated"},
                LoadGlobalInst{"%ret", "@g"},
            },
            ReturnInst{"%ret"},
        }},
    });
    return module;
}

IRModule recursionModule() {
    IRModule module;
    module.functions.push_back({
        "fact",
        Type::Int,
        {{"n", "%p0"}},
        {
            {
                "entry",
                {
                    ConstInst{"%one", 1},
                    LeInst{"%base_cond", "%p0", "%one"},
                },
                BranchInst{"%base_cond", "base_0", "recur_0"},
            },
            {
                "base_0",
                {ConstInst{"%base_ret", 1}},
                ReturnInst{"%base_ret"},
            },
            {
                "recur_0",
                {
                    ConstInst{"%one_recur", 1},
                    SubInst{"%next", "%p0", "%one_recur"},
                    CallInst{"%sub", "fact", {"%next"}},
                    MulInst{"%ret", "%p0", "%sub"},
                },
                ReturnInst{"%ret"},
            },
        },
    });
    module.functions.push_back({
        "main",
        Type::Int,
        {},
        {{
            "entry",
            {
                ConstInst{"%n", 5},
                CallInst{"%ret", "fact", {"%n"}},
            },
            ReturnInst{"%ret"},
        }},
    });
    return module;
}

void printUsage(const char* argv0) {
    std::cerr << "usage: " << argv0 << " --case <name> [--opt]\n"
              << "cases: basic_return, loop_sum, many_params, global_var, recursion\n";
}

IRModule moduleForCase(std::string_view name) {
    if (name == "basic_return") {
        return basicReturnModule();
    }
    if (name == "loop_sum") {
        return loopModule();
    }
    if (name == "many_params") {
        return manyParamsModule();
    }
    if (name == "global_var") {
        return globalVarModule();
    }
    if (name == "recursion") {
        return recursionModule();
    }
    throw std::invalid_argument("unknown contract execution case");
}

} // namespace

int main(int argc, char** argv) {
    std::string caseName;
    BackendOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--case" && i + 1 < argc) {
            caseName = argv[++i];
        } else if (arg == "--opt") {
            options.enableOpt = true;
        } else {
            printUsage(argv[0]);
            return 2;
        }
    }

    if (caseName.empty()) {
        printUsage(argv[0]);
        return 2;
    }

    try {
        std::cout << RiscvBackend().generate(moduleForCase(caseName), options);
    } catch (const std::exception& error) {
        std::cerr << "contract exec case generation failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
