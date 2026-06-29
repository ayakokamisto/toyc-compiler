#include "toyc/target/riscv32/asm_emitter.h"
#include "toyc/mir/mir.h"
#include "toyc/target/riscv32/registers.h"
#include "toyc/target/riscv32/spill_all_allocator.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace toyc::riscv32 {

std::string_view regName(GPRegister reg) {
  switch (reg) {
    case ZERO: return "zero";
    case RA:   return "ra";
    case SP:   return "sp";
    case GP:   return "gp";
    case TP:   return "tp";
    case T0:   return "t0";
    case T1:   return "t1";
    case T2:   return "t2";
    case S0:   return "s0";
    case S1:   return "s1";
    case A0:   return "a0";
    case A1:   return "a1";
    case A2:   return "a2";
    case A3:   return "a3";
    case A4:   return "a4";
    case A5:   return "a5";
    case A6:   return "a6";
    case A7:   return "a7";
    case S2:   return "s2";
    case S3:   return "s3";
    case S4:   return "s4";
    case S5:   return "s5";
    case S6:   return "s6";
    case S7:   return "s7";
    case S8:   return "s8";
    case S9:   return "s9";
    case S10:  return "s10";
    case S11:  return "s11";
    case T3:   return "t3";
    case T4:   return "t4";
    case T5:   return "t5";
    case T6:   return "t6";
  }
  return "???";
}

namespace {

std::string globalLabel(GlobalId id) {
  return ".Ltoyc.global." + std::to_string(id.value);
}

std::string functionLabel(const MIRFunction& func) {
  if (func.name == "main") return "main";
  if (func.name.rfind(".Ltoyc.", 0) == 0) return func.name;
  return ".Ltoyc.fn." + std::to_string(func.sourceFuncId.value);
}

std::string blockLabel(const MIRFunction& func, BlockId id) {
  for (const auto& block : func.blocks) {
    if (block.id == id) return block.label;
  }
  return ".Ltoyc.bb." + std::to_string(id.value);
}

bool fitsI12(int32_t value) {
  return value >= -2048 && value <= 2047;
}

std::string trim(std::string_view text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }
  return std::string(text);
}

std::string stripComment(std::string_view line) {
  const auto comment = line.find('#');
  if (comment != std::string_view::npos) line = line.substr(0, comment);
  return trim(line);
}

std::vector<std::string> splitLines(std::string_view text) {
  std::vector<std::string> lines;
  std::istringstream in{std::string(text)};
  std::string line;
  while (std::getline(in, line)) {
    while (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(std::move(line));
  }
  return lines;
}

std::string joinLines(const std::vector<std::string>& lines) {
  std::ostringstream out;
  for (const auto& line : lines) out << line << "\n";
  return out.str();
}

std::vector<std::string> splitOperands(std::string_view text) {
  std::vector<std::string> operands;
  while (!text.empty()) {
    const auto comma = text.find(',');
    if (comma == std::string_view::npos) {
      operands.push_back(trim(text));
      break;
    }
    operands.push_back(trim(text.substr(0, comma)));
    text.remove_prefix(comma + 1);
  }
  return operands;
}

struct ParsedInst {
  bool instruction = false;
  std::string opcode;
  std::vector<std::string> operands;
};

ParsedInst parseInst(std::string_view line) {
  ParsedInst parsed;
  const std::string text = stripComment(line);
  if (text.empty() || text.back() == ':' || text.front() == '.') return parsed;
  std::string_view view(text);
  const auto space = view.find(' ');
  parsed.opcode = space == std::string_view::npos ? std::string(view)
                                                   : std::string(view.substr(0, space));
  if (space != std::string_view::npos) parsed.operands = splitOperands(view.substr(space + 1));
  parsed.instruction = true;
  return parsed;
}

bool isLabelOrDirective(std::string_view line) {
  const std::string text = stripComment(line);
  return !text.empty() && (text.back() == ':' || text.front() == '.');
}

bool isControlFlow(const ParsedInst& inst) {
  if (!inst.instruction) return false;
  return inst.opcode == "j" || inst.opcode == "ret" || inst.opcode == "call" ||
         (!inst.opcode.empty() && inst.opcode.front() == 'b');
}

std::string definedReg(const ParsedInst& inst) {
  if (!inst.instruction || inst.operands.empty()) return {};
  if (inst.opcode == "sw" || inst.opcode == "j" || inst.opcode == "ret" ||
      inst.opcode == "call" || (!inst.opcode.empty() && inst.opcode.front() == 'b')) {
    return {};
  }
  return inst.operands.front();
}

bool isRedundantMove(const ParsedInst& inst) {
  return inst.opcode == "mv" && inst.operands.size() == 2 && inst.operands[0] == inst.operands[1];
}

bool parseJumpTarget(const ParsedInst& inst, std::string& target) {
  if (inst.opcode != "j" || inst.operands.size() != 1) return false;
  target = inst.operands[0];
  return !target.empty();
}

bool isLabelLine(std::string_view line, std::string_view label) {
  const std::string text = stripComment(line);
  return text.size() == label.size() + 1 && text.back() == ':' &&
         std::string_view(text).substr(0, text.size() - 1) == label;
}

std::string formatInst(const std::string& opcode, const std::vector<std::string>& operands) {
  std::ostringstream out;
  out << "  " << opcode;
  if (!operands.empty()) {
    out << " ";
    for (size_t i = 0; i < operands.size(); ++i) {
      if (i != 0) out << ", ";
      out << operands[i];
    }
  }
  return out.str();
}

bool parseStackSlot(std::string_view operand, std::string& slot) {
  const std::string text = trim(operand);
  const auto open = text.find('(');
  const auto close = text.find(')');
  if (open == std::string::npos || close != text.size() - 1) return false;
  if (text.substr(open + 1, close - open - 1) != "sp") return false;
  const std::string offset = trim(std::string_view(text).substr(0, open));
  if (offset.empty()) return false;
  for (size_t i = 0; i < offset.size(); ++i) {
    if (i == 0 && offset[i] == '-') continue;
    if (!std::isdigit(static_cast<unsigned char>(offset[i]))) return false;
  }
  slot = offset + "(sp)";
  return true;
}

std::string stackForward(std::string assembly) {
  auto lines = splitLines(assembly);
  std::vector<std::string> out;
  out.reserve(lines.size());
  std::unordered_map<std::string, std::string> stackSlots;

  for (const auto& line : lines) {
    auto inst = parseInst(line);
    if (!inst.instruction || isControlFlow(inst) || isLabelOrDirective(line)) {
      stackSlots.clear();
      out.push_back(line);
      continue;
    }

    const auto def = definedReg(inst);
    if (!def.empty()) {
      for (auto it = stackSlots.begin(); it != stackSlots.end();) {
        if (it->second == def) {
          it = stackSlots.erase(it);
        } else {
          ++it;
        }
      }
    }

    if (inst.opcode == "lw" && inst.operands.size() == 2) {
      auto slot = stackSlots.find(inst.operands[1]);
      if (slot != stackSlots.end()) {
        if (inst.operands[0] != slot->second) {
          out.push_back(formatInst("mv", {inst.operands[0], slot->second}));
        }
        continue;
      }
    }

    out.push_back(line);
    if (inst.opcode == "sw" && inst.operands.size() == 2) {
      stackSlots[inst.operands[1]] = inst.operands[0];
    }
  }
  return joinLines(out);
}

std::vector<std::string> eliminateDeadStackStores(const std::vector<std::string>& lines) {
  std::vector<std::string> out = lines;
  std::unordered_set<std::string> liveSlots;
  bool knownLiveness = false;

  for (size_t i = out.size(); i > 0; --i) {
    auto& line = out[i - 1];
    auto inst = parseInst(line);
    if (!inst.instruction || isLabelOrDirective(line)) {
      liveSlots.clear();
      knownLiveness = false;
      continue;
    }
    if (inst.opcode == "ret") {
      liveSlots.clear();
      knownLiveness = true;
      continue;
    }
    if (isControlFlow(inst)) {
      liveSlots.clear();
      knownLiveness = false;
      continue;
    }
    if (inst.opcode == "lw" && inst.operands.size() == 2 && knownLiveness) {
      liveSlots.insert(inst.operands[1]);
      continue;
    }
    if (inst.opcode == "sw" && inst.operands.size() == 2 && knownLiveness) {
      if (!liveSlots.contains(inst.operands[1])) {
        line.clear();
      } else {
        liveSlots.erase(inst.operands[1]);
      }
    }
  }

  out.erase(std::remove(out.begin(), out.end(), ""), out.end());
  return out;
}

std::string peephole(std::string assembly) {
  assembly = stackForward(std::move(assembly));
  auto input = eliminateDeadStackStores(splitLines(assembly));
  std::vector<std::string> pass;
  pass.reserve(input.size());

  for (size_t i = 0; i < input.size(); ++i) {
    auto inst = parseInst(input[i]);
    if (isRedundantMove(inst)) continue;

    std::string jumpTarget;
    if (parseJumpTarget(inst, jumpTarget)) {
      size_t next = i + 1;
      while (next < input.size() && stripComment(input[next]).empty()) ++next;
      if (next < input.size() && isLabelLine(input[next], jumpTarget)) continue;
    }

    pass.push_back(input[i]);
  }

  pass = eliminateDeadStackStores(pass);
  return joinLines(pass);
}

bool isFunctionEntryLabel(std::string_view line) {
  const std::string text = stripComment(line);
  if (text == "main:") return true;
  return text.rfind(".Ltoyc.fn.", 0) == 0 && text.back() == ':';
}

std::unordered_map<std::string, std::string> selectPromotedStackSlots(
    const std::vector<std::string>& lines, size_t begin, size_t end) {
  bool hasCall = false;
  std::unordered_map<std::string, int> counts;
  for (size_t i = begin; i < end; ++i) {
    auto inst = parseInst(lines[i]);
    if (!inst.instruction) continue;
    if (inst.opcode == "call") {
      hasCall = true;
      break;
    }
    if ((inst.opcode == "lw" || inst.opcode == "sw") && inst.operands.size() == 2) {
      std::string slot;
      if (parseStackSlot(inst.operands[1], slot)) ++counts[slot];
    }
  }
  if (hasCall || counts.empty()) return {};

  std::vector<std::pair<std::string, int>> ranked(counts.begin(), counts.end());
  std::sort(ranked.begin(), ranked.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.second != rhs.second) return lhs.second > rhs.second;
    return lhs.first < rhs.first;
  });

  static constexpr const char* regs[] = {"t4", "t5", "t6"};
  std::unordered_map<std::string, std::string> promoted;
  for (size_t i = 0; i < ranked.size() && i < std::size(regs); ++i) {
    if (ranked[i].second < 2) break;
    promoted.emplace(ranked[i].first, regs[i]);
  }
  return promoted;
}

bool functionHasCall(const std::vector<std::string>& lines, size_t begin, size_t end) {
  for (size_t i = begin; i < end; ++i) {
    auto inst = parseInst(lines[i]);
    if (inst.instruction && inst.opcode == "call") return true;
  }
  return false;
}

void removeNeverLoadedStackStores(std::vector<std::string>& lines, size_t begin, size_t end) {
  if (functionHasCall(lines, begin, end)) return;

  std::unordered_set<std::string> loadedSlots;
  for (size_t i = begin; i < end; ++i) {
    auto inst = parseInst(lines[i]);
    if (inst.opcode != "lw" || inst.operands.size() != 2) continue;
    std::string slot;
    if (parseStackSlot(inst.operands[1], slot)) loadedSlots.insert(slot);
  }

  for (size_t i = begin; i < end; ++i) {
    auto inst = parseInst(lines[i]);
    if (inst.opcode != "sw" || inst.operands.size() != 2) continue;
    std::string slot;
    if (!parseStackSlot(inst.operands[1], slot)) continue;
    if (!loadedSlots.contains(slot)) lines[i].clear();
  }
}

void promoteStackSlotsInFunction(std::vector<std::string>& lines, size_t begin, size_t end) {
  const auto promoted = selectPromotedStackSlots(lines, begin, end);
  if (!promoted.empty()) {
    for (size_t i = begin; i < end; ++i) {
      auto inst = parseInst(lines[i]);
      if (!inst.instruction || inst.operands.size() != 2) continue;
      if (inst.opcode != "lw" && inst.opcode != "sw") continue;

      std::string slot;
      if (!parseStackSlot(inst.operands[1], slot)) continue;
      auto it = promoted.find(slot);
      if (it == promoted.end()) continue;

      if (inst.opcode == "lw") {
        if (inst.operands[0] == it->second) {
          lines[i].clear();
        } else {
          lines[i] = formatInst("mv", {inst.operands[0], it->second});
        }
      } else {
        if (inst.operands[0] == it->second) {
          lines[i].clear();
        } else {
          lines[i] = formatInst("mv", {it->second, inst.operands[0]});
        }
      }
    }
  }
  removeNeverLoadedStackStores(lines, begin, end);
}

std::string promoteLeafStackSlots(std::string assembly) {
  auto lines = splitLines(assembly);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (!isFunctionEntryLabel(lines[i])) continue;
    size_t end = i + 1;
    while (end < lines.size() && !isFunctionEntryLabel(lines[end])) ++end;
    promoteStackSlotsInFunction(lines, i + 1, end);
    i = end - 1;
  }

  lines.erase(std::remove(lines.begin(), lines.end(), ""), lines.end());
  return joinLines(lines);
}

class Emitter {
public:
  explicit Emitter(const AllocatedMachineModule& module) : module_(module) {}

  std::string emit() {
    emitData();
    out_ << "\n.section .text\n";
    for (const auto& func : module_.functions) {
      emitFunction(func);
    }
    emitHelpers();
    return out_.str();
  }

private:
  const AllocatedMachineModule& module_;
  std::ostringstream out_;
  const MIRFunction* func_ = nullptr;
  FrameLayout layout_;
  std::unordered_map<uint32_t, int32_t> vregOffsets_;

  // IRSlot → VReg forwarding cache.
  // When a StoreFrame writes to an IRSlot, record which VReg holds the value.
  // When a subsequent LoadFrame reads the same slot, if that VReg is still in
  // a register we skip the lw and emit a mv instead.
  std::unordered_map<int, uint32_t> slotToVReg_;

  // ── VReg register cache (L1 optimization) ────────────────────────────────
  // 3 cache slots using t0, t1, t2.  t3 is reserved for address computation.
  // Dirty entries are written back to stack at block boundaries / calls / ret.
  static constexpr int kCacheSize = 5;
  static constexpr const char* kCacheRegNames[] = {"t0", "t1", "t2", "t4", "t5"};

  struct CacheSlot {
    uint32_t vreg = ~0u;   // VRegId.value; ~0u means empty
    int32_t offset = 0;    // stack offset for writeback
    bool dirty = false;
    int lastAccess = 0;
    bool pinned = false;   // L4: never evict this slot
  };
  CacheSlot cache_[kCacheSize];
  int cacheAccessCounter_ = 0;

  // ── L3: Block-local VReg liveness ──────────────────────────────────────
  // Maps VReg → last instruction index that uses it within the current block.
  // Dead VRegs (lastUse < currentInstIndex_) are preferred for eviction and
  // don't need writeback.
  std::unordered_map<uint32_t, int> vregLastUse_;
  int currentInstIndex_ = 0;

  // ── L4: Loop hot-VReg pinning ──────────────────────────────────────────
  // VRegs that appear frequently in loop bodies are pinned to their cache
  // slots and never evicted.
  std::unordered_set<uint32_t> pinnedVRegs_;

  int cacheFindSlot(uint32_t vreg) const {
    for (int i = 0; i < kCacheSize; ++i) {
      if (cache_[i].vreg == vreg) return i;
    }
    return -1;
  }

  bool cacheSlotFree(int slot) const { return cache_[slot].vreg == ~0u; }

  int cachePickVictim() const {
    for (int i = 0; i < kCacheSize; ++i) {
      if (cacheSlotFree(i)) return i;
    }
    // L4: never evict a pinned slot.
    // L3: prefer dead VRegs (last use already passed) — they don't need writeback.
    //      Among dead VRegs, pick LRU.
    int bestDead = -1;
    for (int i = 0; i < kCacheSize; ++i) {
      if (cache_[i].pinned) continue;
      if (vregIsDead(cache_[i].vreg)) {
        if (bestDead == -1 ||
            cache_[i].lastAccess < cache_[bestDead].lastAccess) {
          bestDead = i;
        }
      }
    }
    if (bestDead >= 0) return bestDead;

    // No dead VRegs available (all are still live). Fall back to LRU among
    // non-pinned entries.
    int best = -1;
    for (int i = 0; i < kCacheSize; ++i) {
      if (cache_[i].pinned) continue;
      if (best == -1 || cache_[i].lastAccess < cache_[best].lastAccess) best = i;
    }
    // If all slots are pinned (shouldn't happen), evict LRU pinned as last resort.
    if (best == -1) {
      for (int i = 0; i < kCacheSize; ++i) {
        if (best == -1 || cache_[i].lastAccess < cache_[best].lastAccess) best = i;
      }
    }
    return best;
  }

  void cacheWriteback(int slot, bool allowRelocate = true) {
    if (cacheSlotFree(slot)) return;
    // L3: dead VRegs don't need writeback — no one will read them.
    bool dead = vregIsDead(cache_[slot].vreg);
    if (cache_[slot].dirty && !dead) {
      int freeSlot = -1;
      if (allowRelocate) {
        for (int i = 0; i < kCacheSize; ++i) {
          if (i != slot && cacheSlotFree(i)) { freeSlot = i; break; }
        }
      }
      if (freeSlot >= 0) {
        out_ << "  mv " << kCacheRegNames[freeSlot] << ", " << kCacheRegNames[slot] << "\n";
        cache_[freeSlot] = cache_[slot];
        cache_[freeSlot].lastAccess = cacheAccessCounter_++;
      } else {
        storeReg(kCacheRegNames[slot], cache_[slot].offset);
      }
    }
    cache_[slot].vreg = ~0u;
    cache_[slot].dirty = false;
  }

  void cacheFlush() {
    // Flush everything, including pinned slots — Call/Return clobbers all regs.
    for (int i = 0; i < kCacheSize; ++i) {
      if (!cacheSlotFree(i) && cache_[i].dirty) {
        storeReg(kCacheRegNames[i], cache_[i].offset);
      }
      cache_[i].vreg = ~0u;
      cache_[i].dirty = false;
      cache_[i].pinned = false;
    }
    slotToVReg_.clear();
  }

  void cacheClear() {
    for (int i = 0; i < kCacheSize; ++i) {
      if (!cache_[i].pinned) {
        cache_[i].vreg = ~0u;
        cache_[i].dirty = false;
      }
    }
    slotToVReg_.clear();
  }

  // ── L3+L4 analysis ─────────────────────────────────────────────────────

  // Collect VReg operands that are USES (not defs) from a MIR instruction.
  void collectVRegUses(const MIRInstruction& inst,
                       std::function<void(uint32_t)> callback) const {
    auto useOp = [&](int idx) {
      if (idx < static_cast<int>(inst.operands.size()) &&
          inst.operands[idx].kind == MIROperandKind::VReg) {
        callback(inst.operands[idx].vregId().value);
      }
    };
    switch (inst.opcode) {
      case MIROpcode::Move:
      case MIROpcode::StoreFrame:
      case MIROpcode::StoreGlobal:
        useOp(1);
        break;
      case MIROpcode::Add: case MIROpcode::Sub: case MIROpcode::Xor:
      case MIROpcode::Or: case MIROpcode::And: case MIROpcode::Sll:
      case MIROpcode::Srl: case MIROpcode::Sra: case MIROpcode::Slt:
      case MIROpcode::Sltu:
        useOp(1); useOp(2);
        break;
      case MIROpcode::Addi: case MIROpcode::Xori: case MIROpcode::Sltiu:
        useOp(1);
        break;
      case MIROpcode::BranchIfNonZero:
        useOp(0);
        break;
      case MIROpcode::Call:
        // Arguments were moved to a0-a7 before the call.
        break;
      default:
        break;
    }
  }

  // Scan a block backwards to find the last instruction index that uses each VReg.
  void computeBlockLiveness(const MIRBlock& block) {
    vregLastUse_.clear();
    for (int i = static_cast<int>(block.insts.size()) - 1; i >= 0; --i) {
      collectVRegUses(block.insts[i], [&](uint32_t vreg) {
        if (!vregLastUse_.contains(vreg)) vregLastUse_[vreg] = i;
      });
    }
  }

  // Check if a VReg is dead at the current instruction position.
  // A VReg is dead if it has no remaining uses in this block (either not
  // in the liveness map at all, or its last use has already passed).
  // VRegs are block-local by construction — cross-block values go through
  // IR slots, not VRegs.
  bool vregIsDead(uint32_t vreg) const {
    auto it = vregLastUse_.find(vreg);
    if (it == vregLastUse_.end()) return true;  // not used in this block
    return it->second < currentInstIndex_;
  }

  // Analyze loops in the function and pin hot VRegs.
  void analyzeLoopsAndPin(const MIRFunction& func) {
    pinnedVRegs_.clear();
    int n = static_cast<int>(func.blocks.size());
    if (n < 2) return;

    // Find back-edges: Branch from block i to block j where j <= i.
    for (int i = 0; i < n; ++i) {
      const auto& insts = func.blocks[i].insts;
      if (insts.empty()) continue;
      const auto& last = insts.back();
      if (last.opcode != MIROpcode::Branch) continue;
      if (last.operands.empty() ||
          last.operands[0].kind != MIROperandKind::BlockLabel) continue;
      BlockId target = last.operands[0].blockLabel();
      for (int j = 0; j <= i; ++j) {
        if (func.blocks[j].id == target) {
          // Back-edge i→j found. Loop is blocks j..i.
          pinLoopVRegs(func, j, i);
          break;
        }
      }
    }
  }

  // Pin the hottest VRegs in a loop (blocks header..latch inclusive).
  void pinLoopVRegs(const MIRFunction& func, int header, int latch) {
    std::unordered_map<uint32_t, int> counts;
    for (int b = header; b <= latch; ++b) {
      for (const auto& inst : func.blocks[b].insts) {
        collectVRegUses(inst, [&](uint32_t vreg) { counts[vreg]++; });
      }
      // Also count VReg defs in StoreFrame (the stored VReg is loop-carried).
      for (const auto& inst : func.blocks[b].insts) {
        if (inst.opcode == MIROpcode::StoreFrame &&
            inst.operands.size() > 1 &&
            inst.operands[1].kind == MIROperandKind::VReg) {
          counts[inst.operands[1].vregId().value] += 2;  // bonus for being stored
        }
      }
    }
    // Pin top 3 with at least 2 uses.
    std::vector<std::pair<uint32_t, int>> ranked(counts.begin(), counts.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    for (size_t k = 0; k < ranked.size() && k < 3; ++k) {
      if (ranked[k].second >= 2) pinnedVRegs_.insert(ranked[k].first);
    }
  }

  // Ensure a physical register is free for direct use (evict cache entry if needed).
  void cacheEnsureFree(const std::string& reg) {
    for (int i = 0; i < kCacheSize; ++i) {
      if (kCacheRegNames[i] == reg && !cacheSlotFree(i)) {
        cacheWriteback(i);
        return;
      }
    }
  }

  // Load a VReg into a cache register and return the register name.
  std::string cacheLoadVReg(VRegId vreg) {
    int existing = cacheFindSlot(vreg.value);
    if (existing >= 0) {
      cache_[existing].lastAccess = cacheAccessCounter_++;
      return kCacheRegNames[existing];
    }
    int slot = cachePickVictim();
    cacheWriteback(slot);
    int32_t offset = vregOffset(vreg);
    loadReg(kCacheRegNames[slot], offset);
    cache_[slot].vreg = vreg.value;
    cache_[slot].offset = offset;
    cache_[slot].dirty = false;
    cache_[slot].lastAccess = cacheAccessCounter_++;
    cache_[slot].pinned = pinnedVRegs_.contains(vreg.value);
    return kCacheRegNames[slot];
  }

  // Record that a VReg's value is now in srcReg (from a computation).
  // Uses lazy writeback: marks dirty, defers stack store until eviction
  // or function exit.  Slot promotion (t4-t6) handles cross-block IR slots,
  // so VRegs only need intra-block liveness — no flush before branches.
  void cacheDefineVReg(VRegId vreg, const std::string& srcReg) {
    int32_t offset = vregOffset(vreg);

    bool pin = pinnedVRegs_.contains(vreg.value);

    // If this VReg is already cached in a different slot, evict the old slot.
    int existing = cacheFindSlot(vreg.value);
    if (existing >= 0) {
      std::string cachedReg = kCacheRegNames[existing];
      if (srcReg != cachedReg) {
        out_ << "  mv " << cachedReg << ", " << srcReg << "\n";
      }
      cache_[existing].dirty = true;
      cache_[existing].lastAccess = cacheAccessCounter_++;
      cache_[existing].pinned = pin;
      return;
    }

    // Determine which slot the source register maps to.
    int srcSlot = -1;
    for (int i = 0; i < kCacheSize; ++i) {
      if (kCacheRegNames[i] == srcReg) { srcSlot = i; break; }
    }

    if (srcSlot >= 0) {
      // srcReg is a cache register. Ensure it's free for this VReg.
      cacheWriteback(srcSlot);
      cache_[srcSlot].vreg = vreg.value;
      cache_[srcSlot].offset = offset;
      cache_[srcSlot].dirty = true;
      cache_[srcSlot].lastAccess = cacheAccessCounter_++;
      cache_[srcSlot].pinned = pin;
    } else {
      // srcReg is not a cache register (e.g., "a0" from call return, or "t3").
      // Pick a free slot, evict if needed, and mv into it.
      int slot = cachePickVictim();
      cacheWriteback(slot);
      out_ << "  mv " << kCacheRegNames[slot] << ", " << srcReg << "\n";
      cache_[slot].vreg = vreg.value;
      cache_[slot].offset = offset;
      cache_[slot].dirty = true;
      cache_[slot].lastAccess = cacheAccessCounter_++;
      cache_[slot].pinned = pin;
    }
  }

  std::string labelForGlobal(GlobalId id) const {
    for (const auto& global : module_.globals) {
      if (global.id == id && global.name.rfind(".Ltoyc.", 0) == 0) return global.name;
    }
    return globalLabel(id);
  }

  void emitData() {
    out_ << ".section .data\n.align 2\n";
    for (const auto& global : module_.globals) {
      out_ << labelForGlobal(global.id) << ":\n";
      out_ << "  .word " << global.staticInitialValue << "\n";
    }
  }

  void emitFunction(const AllocatedMachineFunction& allocatedFunction) {
    const auto& func = allocatedFunction.function;
    func_ = &func;
    layout_ = allocatedFunction.frameLayout;
    vregOffsets_.clear();
    for (const auto& object : func.frameObjects) {
      if (object.kind == FrameObjectKind::VRegHome && object.vregId.has_value()) {
        vregOffsets_[object.vregId->value] = object.offset;
      }
    }

    cacheClear();

    // L4: analyze loops and pin hot VRegs before emitting any blocks.
    analyzeLoopsAndPin(func);

    out_ << "\n";
    if (func.name == "main") {
      out_ << ".globl main\n";
    }
    out_ << functionLabel(func) << ":\n";
    adjustSp(-layout_.totalSize);
    if (func.hasCall) {
      const auto* ra = savedRaObject(func);
      if (ra != nullptr) storeReg("ra", ra->offset);
    }
    spillIncomingParameters(func);

    for (const auto& block : func.blocks) {
      out_ << block.label << ":\n";
      // L3: pre-scan this block for VReg liveness.
      computeBlockLiveness(block);
      currentInstIndex_ = 0;
      for (const auto& inst : block.insts) {
        emitInstruction(inst);
        ++currentInstIndex_;
      }
    }
  }

  const FrameObject* frameObject(int index) const {
    if (index < 0 || index >= static_cast<int>(func_->frameObjects.size())) return nullptr;
    return &func_->frameObjects[static_cast<size_t>(index)];
  }

  const FrameObject* savedRaObject(const MIRFunction& func) const {
    for (const auto& object : func.frameObjects) {
      if (object.kind == FrameObjectKind::SavedReturnAddress) return &object;
    }
    return nullptr;
  }

  int32_t vregOffset(VRegId id) const {
    auto it = vregOffsets_.find(id.value);
    return it == vregOffsets_.end() ? 0 : it->second;
  }

  void adjustSp(int32_t amount) {
    if (amount == 0) return;
    if (fitsI12(amount)) {
      out_ << "  addi sp, sp, " << amount << "\n";
      return;
    }
    out_ << "  li t3, " << amount << "\n";
    out_ << "  add sp, sp, t3\n";
  }

  void loadReg(const std::string& reg, int32_t offset) {
    if (fitsI12(offset)) {
      out_ << "  lw " << reg << ", " << offset << "(sp)\n";
      return;
    }
    out_ << "  li t3, " << offset << "\n";
    out_ << "  add t3, sp, t3\n";
    out_ << "  lw " << reg << ", 0(t3)\n";
  }

  void storeReg(const std::string& reg, int32_t offset) {
    if (fitsI12(offset)) {
      out_ << "  sw " << reg << ", " << offset << "(sp)\n";
      return;
    }
    out_ << "  li t3, " << offset << "\n";
    out_ << "  add t3, sp, t3\n";
    out_ << "  sw " << reg << ", 0(t3)\n";
  }

  std::string loadOperand(const MIROperand& operand, const std::string& scratch) {
    switch (operand.kind) {
      case MIROperandKind::VReg:
        return cacheLoadVReg(operand.vregId());
      case MIROperandKind::PhysReg:
        return std::string(physRegName(operand.physReg()));
      case MIROperandKind::Immediate:
        cacheEnsureFree(scratch);
        out_ << "  li " << scratch << ", " << operand.imm() << "\n";
        return scratch;
      default:
        return scratch;
    }
  }

  void storeDestination(const MIROperand& dst, const std::string& reg) {
    if (dst.kind == MIROperandKind::VReg) {
      cacheDefineVReg(dst.vregId(), reg);
      return;
    }
    if (dst.kind == MIROperandKind::PhysReg) {
      const auto name = std::string(physRegName(dst.physReg()));
      if (name != reg) out_ << "  mv " << name << ", " << reg << "\n";
    }
  }

  void spillIncomingParameters(const MIRFunction& func) {
    static constexpr const char* argRegs[] = {
      "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
    };
    for (size_t i = 0; i < func.parameterVRegs.size(); ++i) {
      int32_t dst = vregOffset(func.parameterVRegs[i]);
      if (i < 8) {
        storeReg(argRegs[i], dst);
      } else {
        loadReg("t0", layout_.incomingArgOffset(static_cast<int>(i)));
        storeReg("t0", dst);
      }
    }
  }

  void emitInstruction(const MIRInstruction& inst) {
    switch (inst.opcode) {
      case MIROpcode::Comment:
        out_ << "  # " << inst.comment << "\n";
        break;
      case MIROpcode::LoadImm:
      case MIROpcode::Li:
        out_ << "  li t3, " << inst.operands[1].imm() << "\n";
        storeDestination(inst.operands[0], "t3");
        break;
      case MIROpcode::Move: {
        std::string src = loadOperand(inst.operands[1], "t0");
        storeDestination(inst.operands[0], src);
        break;
      }
      case MIROpcode::LoadFrame: {
        const auto* object = frameObject(inst.operands[1].frameSlotIndex());
        int foIndex = inst.operands[1].frameSlotIndex();

        // Check if this slot's value is still in a register from a prior StoreFrame.
        auto slotIt = slotToVReg_.find(foIndex);
        if (slotIt != slotToVReg_.end()) {
          int cachedSlot = cacheFindSlot(slotIt->second);
          if (cachedSlot >= 0) {
            // Value still in register.  Pick a DIFFERENT free register and mv
            // into it, so the old and new VRegs don't alias the same register.
            std::string srcReg = kCacheRegNames[cachedSlot];
            int dstSlot = -1;
            for (int i = 0; i < kCacheSize; ++i) {
              if (i != cachedSlot && cacheSlotFree(i)) { dstSlot = i; break; }
            }
            if (dstSlot >= 0) {
              out_ << "  mv " << kCacheRegNames[dstSlot] << ", " << srcReg << "\n";
              storeDestination(inst.operands[0], kCacheRegNames[dstSlot]);
            } else {
              // No free slot — reuse srcReg (fallback, correct for same-value copy).
              cache_[cachedSlot].lastAccess = cacheAccessCounter_++;
              storeDestination(inst.operands[0], srcReg);
            }
            break;
          }
        }

        loadReg("t3", object ? object->offset : 0);
        storeDestination(inst.operands[0], "t3");
        break;
      }
      case MIROpcode::StoreFrame: {
        const auto* object = frameObject(inst.operands[0].frameSlotIndex());
        int foIndex = inst.operands[0].frameSlotIndex();
        std::string src = loadOperand(inst.operands[1], "t0");

        // Record that this IRSlot's value now lives in the source VReg.
        if (inst.operands[1].kind == MIROperandKind::VReg) {
          slotToVReg_[foIndex] = inst.operands[1].vregId().value;
        }

        storeReg(src, object ? object->offset : 0);
        break;
      }
      case MIROpcode::LoadGlobal:
        out_ << "  la t3, " << labelForGlobal(inst.operands[1].globalId()) << "\n";
        out_ << "  lw t3, 0(t3)\n";
        storeDestination(inst.operands[0], "t3");
        break;
      case MIROpcode::StoreGlobal: {
        std::string src = loadOperand(inst.operands[1], "t0");
        out_ << "  la t3, " << labelForGlobal(inst.operands[0].globalId()) << "\n";
        out_ << "  sw " << src << ", 0(t3)\n";
        break;
      }
      case MIROpcode::Add:
      case MIROpcode::Sub:
      case MIROpcode::Xor:
      case MIROpcode::Or:
      case MIROpcode::And:
      case MIROpcode::Sll:
      case MIROpcode::Srl:
      case MIROpcode::Sra:
      case MIROpcode::Slt:
      case MIROpcode::Sltu:
        emitBinary(inst);
        break;
      case MIROpcode::Addi:
      case MIROpcode::Xori:
      case MIROpcode::Sltiu:
        emitImmediate(inst);
        break;
      case MIROpcode::Call:
        // Call clobbers t0-t6, a0-a7. Flush dirty entries, then clear.
        cacheFlush();
        out_ << "  call " << inst.comment << "\n";
        cacheClear();
        break;
      case MIROpcode::Return:
        // Write back dirty VRegs before function exit.
        cacheFlush();
        emitReturn();
        break;
      case MIROpcode::Branch:
        out_ << "  j " << blockLabel(*func_, inst.operands[0].blockLabel()) << "\n";
        break;
      case MIROpcode::BranchIfNonZero: {
        std::string cond = loadOperand(inst.operands[0], "t0");
        out_ << "  bnez " << cond << ", " << blockLabel(*func_, inst.operands[1].blockLabel()) << "\n";
        break;
      }
      case MIROpcode::La:
        out_ << "  la t3, " << labelForGlobal(inst.operands[1].globalId()) << "\n";
        storeDestination(inst.operands[0], "t3");
        break;
    }
  }

  // Pick the best result register for a binary/immediate operation.
  // Avoids clobbering a live VReg in t2 when a free/dead slot exists.
  std::string pickResultReg(const std::string& lhs, const std::string& rhs) {
    auto used = [&](const std::string& r) { return r == lhs || r == rhs; };
    // 1. Free slot whose register is not an operand.
    for (int i = 0; i < kCacheSize; ++i) {
      if (cacheSlotFree(i) && !used(kCacheRegNames[i])) return kCacheRegNames[i];
    }
    // 2. Dead VReg slot (can be evicted without writeback).
    for (int i = 0; i < kCacheSize; ++i) {
      if (!used(kCacheRegNames[i]) && vregIsDead(cache_[i].vreg)) return kCacheRegNames[i];
    }
    // 3. Any unused register (live VReg will be written back).
    for (int i = 0; i < kCacheSize; ++i) {
      if (!used(kCacheRegNames[i])) return kCacheRegNames[i];
    }
    // 4. All cache regs are operands.  t2 works (RV32 allows dst == src).
    return "t2";
  }

  void emitBinary(const MIRInstruction& inst) {
    std::string lhs = loadOperand(inst.operands[1], "t0");
    std::string rhs = loadOperand(inst.operands[2], "t1");
    std::string dst = pickResultReg(lhs, rhs);
    cacheEnsureFree(dst);
    out_ << "  " << mirOpcodeName(inst.opcode) << " " << dst << ", " << lhs << ", " << rhs << "\n";
    storeDestination(inst.operands[0], dst);
  }

  void emitImmediate(const MIRInstruction& inst) {
    std::string lhs = loadOperand(inst.operands[1], "t0");
    int32_t imm = inst.operands[2].imm();
    std::string dst = pickResultReg(lhs, "");  // rhs is not a register for I-type
    if (fitsI12(imm)) {
      cacheEnsureFree(dst);
      out_ << "  " << mirOpcodeName(inst.opcode) << " " << dst << ", " << lhs << ", " << imm << "\n";
    } else {
      // Use t3 for the large immediate to avoid clobbering a cached register.
      out_ << "  li t3, " << imm << "\n";
      const char* op = inst.opcode == MIROpcode::Addi ? "add" :
                       inst.opcode == MIROpcode::Xori ? "xor" : "sltu";
      cacheEnsureFree(dst);
      out_ << "  " << op << " " << dst << ", " << lhs << ", t3\n";
    }
    storeDestination(inst.operands[0], dst);
  }

  void emitReturn() {
    if (func_->hasCall) {
      const auto* ra = savedRaObject(*func_);
      if (ra != nullptr) loadReg("ra", ra->offset);
    }
    adjustSp(layout_.totalSize);
    out_ << "  ret\n";
  }

  bool textUses(const std::string& needle) const {
    for (const auto& allocatedFunction : module_.functions) {
      for (const auto& block : allocatedFunction.function.blocks) {
        for (const auto& inst : block.insts) {
          if (inst.opcode == MIROpcode::Call && inst.comment == needle) return true;
        }
      }
    }
    return false;
  }

  void emitHelpers() {
    if (textUses(".Ltoyc.mul_i32")) emitMulHelper();
    if (textUses(".Ltoyc.div_i32")) emitDivHelper();
    if (textUses(".Ltoyc.rem_i32")) emitRemHelper();
  }

  void emitMulHelper() {
    out_ << R"ASM(
.Ltoyc.mul_i32:
  mv t0, a0
  mv t1, a1
  li t2, 0
.Ltoyc.mul_i32.loop:
  andi t3, t1, 1
  beqz t3, .Ltoyc.mul_i32.skip
  add t2, t2, t0
.Ltoyc.mul_i32.skip:
  slli t0, t0, 1
  srli t1, t1, 1
  bnez t1, .Ltoyc.mul_i32.loop
  mv a0, t2
  ret
)ASM";
  }

  void emitUnsignedDivCore(const std::string& suffix) {
    out_ << ".Ltoyc.udiv_core_" << suffix << ":\n"
         << "  li t2, 0\n"
         << "  li t3, 0\n"
         << "  li t4, 32\n"
         << ".Ltoyc.udiv_core_" << suffix << ".loop:\n"
         << "  slli t3, t3, 1\n"
         << "  srli t5, t0, 31\n"
         << "  or t3, t3, t5\n"
         << "  slli t0, t0, 1\n"
         << "  slli t2, t2, 1\n"
         << "  bltu t3, t1, .Ltoyc.udiv_core_" << suffix << ".skip\n"
         << "  sub t3, t3, t1\n"
         << "  ori t2, t2, 1\n"
         << ".Ltoyc.udiv_core_" << suffix << ".skip:\n"
         << "  addi t4, t4, -1\n"
         << "  bnez t4, .Ltoyc.udiv_core_" << suffix << ".loop\n";
  }

  void emitDivHelper() {
    out_ << R"ASM(
.Ltoyc.div_i32:
  beqz a1, .Ltoyc.div_i32.zero
  xor t6, a0, a1
  mv t0, a0
  bgez t0, .Ltoyc.div_i32.lhs_abs
  sub t0, zero, t0
.Ltoyc.div_i32.lhs_abs:
  mv t1, a1
  bgez t1, .Ltoyc.div_i32.rhs_abs
  sub t1, zero, t1
.Ltoyc.div_i32.rhs_abs:
)ASM";
    emitUnsignedDivCore("div");
    out_ << R"ASM(
  bgez t6, .Ltoyc.div_i32.done
  sub t2, zero, t2
.Ltoyc.div_i32.done:
  mv a0, t2
  ret
.Ltoyc.div_i32.zero:
  li a0, 0
  ret
)ASM";
  }

  void emitRemHelper() {
    out_ << R"ASM(
.Ltoyc.rem_i32:
  beqz a1, .Ltoyc.rem_i32.zero
  mv t6, a0
  mv t0, a0
  bgez t0, .Ltoyc.rem_i32.lhs_abs
  sub t0, zero, t0
.Ltoyc.rem_i32.lhs_abs:
  mv t1, a1
  bgez t1, .Ltoyc.rem_i32.rhs_abs
  sub t1, zero, t1
.Ltoyc.rem_i32.rhs_abs:
)ASM";
    emitUnsignedDivCore("rem");
    out_ << R"ASM(
  bgez t6, .Ltoyc.rem_i32.done
  sub t3, zero, t3
.Ltoyc.rem_i32.done:
  mv a0, t3
  ret
.Ltoyc.rem_i32.zero:
  li a0, 0
  ret
)ASM";
  }
};

} // namespace

std::string emitAssembly(const AllocatedMachineModule& module, bool optimize) {
  auto assembly = Emitter(module).emit();
  if (optimize) {
    // L1 VReg cache (in Emitter) handles VReg-to-VReg forwarding using t0-t2.
    // Text-level peephole passes are disabled for now — they operate on
    // assembly text without awareness of the VReg cache state and can
    // introduce incorrect register reuse when combined with lazy writeback.
    // TODO: re-enable peephole after making it cache-aware, or replace with
    // MIR-level passes (L2/L3/L4 in the backend optimization plan).
    (void)promoteLeafStackSlots;
    (void)peephole;
  }
  return assembly;
}

} // namespace toyc::riscv32
