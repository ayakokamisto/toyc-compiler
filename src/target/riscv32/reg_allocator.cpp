/// RV32I linear-scan register allocator.
///
/// The active implementation uses only s1-s11 for virtual registers. t*/a*
/// remain scratch, argument, and return registers.

#include "toyc/target/riscv32/reg_allocator.h"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace toyc::riscv32 {

namespace {

constexpr const char* kCalleeSaved[] = {
    "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"};

struct Interval {
  uint32_t vreg = 0;
  int start = std::numeric_limits<int>::max();
  int end = -1;
  std::string reg;
};

void touchInterval(std::unordered_map<uint32_t, Interval>& intervals, uint32_t vreg, int pos) {
  auto& interval = intervals[vreg];
  interval.vreg = vreg;
  interval.start = std::min(interval.start, pos);
  interval.end = std::max(interval.end, pos);
}

std::vector<Interval> buildIntervals(const MIRFunction& function) {
  std::unordered_map<uint32_t, Interval> intervals;
  int pos = 0;

  for (auto vreg : function.parameterVRegs) {
    touchInterval(intervals, vreg.value, pos);
  }

  for (const auto& block : function.blocks) {
    for (const auto& inst : block.insts) {
      for (const auto& operand : inst.operands) {
        if (operand.kind == MIROperandKind::VReg) {
          touchInterval(intervals, operand.vregId().value, pos);
        }
      }
      ++pos;
    }
  }

  std::vector<Interval> ordered;
  ordered.reserve(intervals.size());
  for (auto& [unused, interval] : intervals) {
    (void)unused;
    if (interval.end >= 0) ordered.push_back(interval);
  }

  std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.start != rhs.start) return lhs.start < rhs.start;
    return lhs.end < rhs.end;
  });
  return ordered;
}

void addSavedCalleeObject(MIRFunction& function, const std::string& reg) {
  auto exists = [&](const FrameObject& object) {
    return object.kind == FrameObjectKind::SavedCalleeSaved && object.physReg == reg;
  };
  if (std::find_if(function.frameObjects.begin(), function.frameObjects.end(), exists) !=
      function.frameObjects.end()) {
    return;
  }

  FrameObject saved;
  saved.kind = FrameObjectKind::SavedCalleeSaved;
  saved.size = 4;
  saved.physReg = reg;
  function.addFrameObject(std::move(saved));
}

void assignLinearScan(MIRFunction& function, RegAssignment& assignment) {
  auto intervals = buildIntervals(function);
  std::vector<Interval*> active;
  std::vector<std::string> freeRegs(std::begin(kCalleeSaved), std::end(kCalleeSaved));

  auto sortActive = [&] {
    std::sort(active.begin(), active.end(), [](const auto* lhs, const auto* rhs) {
      return lhs->end < rhs->end;
    });
  };

  auto expireOld = [&](int start) {
    for (auto it = active.begin(); it != active.end();) {
      if ((*it)->end < start) {
        freeRegs.push_back((*it)->reg);
        it = active.erase(it);
      } else {
        ++it;
      }
    }
    std::sort(freeRegs.begin(), freeRegs.end());
  };

  for (auto& current : intervals) {
    expireOld(current.start);

    if (!freeRegs.empty()) {
      current.reg = freeRegs.front();
      freeRegs.erase(freeRegs.begin());
      assignment[current.vreg] = current.reg;
      active.push_back(&current);
      sortActive();
      continue;
    }

    auto spillIt = std::max_element(active.begin(), active.end(), [](const auto* lhs, const auto* rhs) {
      return lhs->end < rhs->end;
    });
    if (spillIt != active.end() && (*spillIt)->end > current.end) {
      current.reg = (*spillIt)->reg;
      assignment.erase((*spillIt)->vreg);
      *spillIt = &current;
      assignment[current.vreg] = current.reg;
      sortActive();
    }
  }

  std::unordered_set<std::string> usedRegs;
  for (const auto& [unused, reg] : assignment) {
    (void)unused;
    usedRegs.insert(reg);
  }
  std::vector<std::string> orderedUsed(usedRegs.begin(), usedRegs.end());
  std::sort(orderedUsed.begin(), orderedUsed.end());
  for (const auto& reg : orderedUsed) addSavedCalleeObject(function, reg);
}

void normalizeSavedReturnAddress(MIRFunction& function) {
  auto isRa = [](const FrameObject& object) {
    return object.kind == FrameObjectKind::SavedReturnAddress;
  };

  if (!function.hasCall) {
    function.frameObjects.erase(
        std::remove_if(function.frameObjects.begin(), function.frameObjects.end(), isRa),
        function.frameObjects.end());
    return;
  }

  if (std::find_if(function.frameObjects.begin(), function.frameObjects.end(), isRa) ==
      function.frameObjects.end()) {
    FrameObject ra;
    ra.kind = FrameObjectKind::SavedReturnAddress;
    ra.size = 4;
    function.addFrameObject(std::move(ra));
  }
}

} // namespace

AllocatedMachineModule RegisterAllocator::allocate(MIRModule module) {
  AllocatedMachineModule allocated;
  allocated.globals = std::move(module.globals);

  for (auto& func : module.functions) {
    AllocatedMachineFunction af;
    af.function = std::move(func);
    auto& mf = af.function;

    if (enableOpt_) {
      assignLinearScan(mf, af.regAssignment);
    }

    normalizeSavedReturnAddress(mf);
    af.frameLayout = FrameLayout::compute(mf);
    allocated.functions.push_back(std::move(af));
  }

  return allocated;
}

} // namespace toyc::riscv32
