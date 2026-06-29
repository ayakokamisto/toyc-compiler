#pragma once

#include "codegen/ContractIR.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace toyc::codegen {

struct VRegProgramPoint {
    int position = 0;
    std::string blockLabel;
    std::vector<std::string> defs;
    std::vector<std::string> uses;
};

struct LiveInterval {
    std::string vreg;
    int start = 0;
    int end = 0;
    int useCount = 0;
    int spillWeight = 0;
    int callCrossingCount = 0;
    bool loopCarried = false;
};

struct VRegAnalysis {
    std::vector<std::string> discoveryOrder;
    std::map<std::string, int, std::less<>> useCounts;
    std::map<std::string, int, std::less<>> accessWeights;
    std::vector<VRegProgramPoint> programPoints;
    std::vector<LiveInterval> liveIntervals;
    std::map<std::string, int, std::less<>> blockLoopDepths;
    std::map<std::string, std::set<std::string, std::less<>>, std::less<>> liveIns;
    std::map<std::string, std::set<std::string, std::less<>>, std::less<>> liveOuts;
    std::set<std::string, std::less<>> loopCarriedVRegs;
    int maxOutgoingArgBytes = 0;
};

[[nodiscard]] VRegAnalysis analyzeVRegs(const contract::IRFunction& function);

} // namespace toyc::codegen
