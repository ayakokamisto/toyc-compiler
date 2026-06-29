/// Linear-scan register allocator for RV32I.

#include "toyc/target/riscv32/reg_allocator.h"
#include "toyc/mir/mir_liveness.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace toyc::riscv32 {

namespace {

constexpr std::array<std::string_view, 5> kCallerSaved = {"t2","t3","t4","t5","t6"};
constexpr std::array<std::string_view, 11> kCalleeSaved = {
    "s1","s2","s3","s4","s5","s6","s7","s8","s9","s10","s11"};
constexpr int kCallCrossWeight = 25;
constexpr int kLoopWeight = 30;
constexpr int kCallerMaxWeight = 5;

bool isCallerSaved(std::string_view r) {
  for (auto s : kCallerSaved) if (s == r) return true;
  return false;
}

struct Active { LiveInterval interval; std::string reg; };

void expireOld(const LiveInterval& cur, std::vector<Active>& active,
               std::vector<std::string>& freeRegs) {
  for (auto it = active.begin(); it != active.end(); ) {
    if (it->interval.end < cur.start) {
      freeRegs.push_back(it->reg);
      it = active.erase(it);
    } else ++it;
  }
}

bool lessValuable(const LiveInterval& a, const LiveInterval& b) {
  int ca = a.spillWeight + a.callCrossingCount * kCallCrossWeight -
           std::max(0, a.end - a.start) / 4;
  int cb = b.spillWeight + b.callCrossingCount * kCallCrossWeight -
           std::max(0, b.end - b.start) / 4;
  if (ca != cb) return ca < cb;
  return a.end > b.end;
}

RegAssignment linearScan(const MIRLiveness& liveness) {
  std::vector<LiveInterval> intervals;
  for (auto& iv : liveness.intervals) {
    bool calleeOk = iv.useCount >= 2 || iv.callCrossingCount > 0 ||
                    iv.spillWeight >= kLoopWeight;
    bool callerOk = iv.useCount >= 1 && iv.spillWeight <= kCallerMaxWeight &&
                    iv.callCrossingCount == 0 && !iv.loopCarried;
    if (calleeOk || callerOk) intervals.push_back(iv);
  }
  std::sort(intervals.begin(), intervals.end(),
            [](const LiveInterval& a, const LiveInterval& b) {
              if (a.start != b.start) return a.start < b.start;
              return a.end < b.end;
            });

  std::vector<std::string> freeRegs;
  for (auto s : kCallerSaved) freeRegs.emplace_back(s);
  for (auto s : kCalleeSaved) freeRegs.emplace_back(s);

  std::vector<Active> active;
  RegAssignment assigned;

  for (auto& iv : intervals) {
    expireOld(iv, active, freeRegs);

    if (!freeRegs.empty()) {
      std::string reg;
      if (iv.callCrossingCount > 0 || iv.loopCarried) {
        auto it = std::find_if(freeRegs.begin(), freeRegs.end(),
                               [](std::string_view r) { return !isCallerSaved(r); });
        if (it != freeRegs.end()) { reg = *it; freeRegs.erase(it); }
      }
      if (reg.empty()) { reg = freeRegs.front(); freeRegs.erase(freeRegs.begin()); }
      assigned[iv.vreg] = reg;
      active.push_back({iv, reg});
      std::sort(active.begin(), active.end(),
                [](const Active& a, const Active& b) { return a.interval.end < b.interval.end; });
      continue;
    }

    auto spillIt = std::min_element(active.begin(), active.end(),
        [](const Active& a, const Active& b) { return lessValuable(a.interval, b.interval); });
    if (spillIt == active.end() || !lessValuable(spillIt->interval, iv)) continue;
    assigned.erase(spillIt->interval.vreg);
    assigned[iv.vreg] = spillIt->reg;
    spillIt->interval = iv;
    std::sort(active.begin(), active.end(),
              [](const Active& a, const Active& b) { return a.interval.end < b.interval.end; });
  }

  return assigned;
}

bool functionHasCall(const MIRFunction& func) {
  for (auto& b : func.blocks)
    for (auto& i : b.insts)
      if (i.opcode == MIROpcode::Call) return true;
  return false;
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
      MIRLiveness liveness = analyzeMIRLiveness(mf);
      RegAssignment assignment = linearScan(liveness);

      // Track callee-saved registers that need save/restore.
      std::vector<std::string> usedCalleeSaved;
      for (auto& [vreg, reg] : assignment) {
        af.regAssignment[vreg] = reg;
        if (!isCallerSaved(reg) &&
            std::find(usedCalleeSaved.begin(), usedCalleeSaved.end(), reg) ==
                usedCalleeSaved.end())
          usedCalleeSaved.push_back(reg);
      }

      // Remove VRegHomes for VRegs that got physical registers.
      std::vector<int> removeIndices;
      for (int i = 0; i < static_cast<int>(mf.frameObjects.size()); ++i) {
        auto& fo = mf.frameObjects[i];
        if (fo.kind == FrameObjectKind::VRegHome && fo.vregId.has_value() &&
            assignment.contains(fo.vregId->value))
          removeIndices.push_back(i);
      }
      std::sort(removeIndices.begin(), removeIndices.end(), std::greater<int>());
      for (int idx : removeIndices)
        mf.frameObjects.erase(mf.frameObjects.begin() + idx);

      // Add callee-saved register save slots.
      for (auto& reg : usedCalleeSaved) {
        FrameObject fo;
        fo.kind = FrameObjectKind::SavedReturnAddress;  // reuse: saved register
        fo.size = 4;
        mf.addFrameObject(fo);
      }
      // Also add saved ra if there are calls.
      if (functionHasCall(mf)) {
        bool hasRa = false;
        for (auto& fo : mf.frameObjects)
          if (fo.kind == FrameObjectKind::SavedReturnAddress) hasRa = true;
        if (!hasRa) {
          FrameObject ra;
          ra.kind = FrameObjectKind::SavedReturnAddress;
          ra.size = 4;
          mf.addFrameObject(ra);
        }
      }
    }

    // Normalize saved ra.
    {
      auto isSavedRa = [](const FrameObject& o) {
        return o.kind == FrameObjectKind::SavedReturnAddress;
      };
      if (!mf.hasCall) {
        mf.frameObjects.erase(
            std::remove_if(mf.frameObjects.begin(), mf.frameObjects.end(), isSavedRa),
            mf.frameObjects.end());
      } else {
        if (std::find_if(mf.frameObjects.begin(), mf.frameObjects.end(), isSavedRa) ==
            mf.frameObjects.end()) {
          FrameObject ra;
          ra.kind = FrameObjectKind::SavedReturnAddress;
          ra.size = 4;
          mf.addFrameObject(ra);
        }
      }
    }

    af.frameLayout = FrameLayout::compute(mf);
    allocated.functions.push_back(std::move(af));
  }

  return allocated;
}

} // namespace toyc::riscv32
