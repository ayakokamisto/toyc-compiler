#include "toyc/mir/mir.h"
#include "toyc/mir/verifier.h"
#include "toyc/target/riscv32/spill_all_allocator.h"

#include <gtest/gtest.h>
#include <sstream>

namespace toyc {

TEST(MIRBasicTest, VRegIdsAreUnique) {
  MIRFunction func;
  auto a = func.allocVReg();
  auto b = func.allocVReg();
  EXPECT_NE(a, b);
  EXPECT_EQ(a.value, 0u);
  EXPECT_EQ(b.value, 1u);
}

TEST(MIRBasicTest, FrameLayoutAssignsDisjointSlotsAndSavedRa) {
  MIRFunction func;
  func.hasCall = true;
  func.maxOutgoingArgCount = 10;

  FrameObject irSlot;
  irSlot.kind = FrameObjectKind::IRSlot;
  irSlot.irSlot = SlotId(0);
  func.addFrameObject(irSlot);

  FrameObject outArg;
  outArg.kind = FrameObjectKind::OutgoingArgument;
  outArg.paramIndex = 8;
  func.addFrameObject(outArg);

  FrameObject home;
  home.kind = FrameObjectKind::VRegHome;
  home.vregId = VRegId(0);
  func.addFrameObject(home);

  FrameObject ra;
  ra.kind = FrameObjectKind::SavedReturnAddress;
  func.addFrameObject(ra);

  auto layout = FrameLayout::compute(func);
  EXPECT_EQ(layout.outgoingArgSize, 8);
  EXPECT_EQ(layout.totalSize % 16, 0);
  EXPECT_EQ(func.frameObjects[1].offset, 0);
  EXPECT_EQ(func.frameObjects[0].offset, 8);
  EXPECT_EQ(func.frameObjects[2].offset, 12);
  EXPECT_EQ(func.frameObjects[3].offset, 16);
  EXPECT_EQ(layout.incomingArgOffset(8), layout.totalSize);
}

TEST(MIRBasicTest, VerifierRejectsInvalidBranchTarget) {
  MIRModule module;
  MIRFunction func;
  func.sourceFuncId = FunctionId(0);
  func.name = "main";

  MIRBlock block;
  block.id = BlockId(0);
  block.label = ".Lbb.0";
  block.insts.push_back(MIRInstruction::make(
      MIROpcode::Branch, {MIROperand::makeBlockLabel(BlockId(99))}));
  func.blocks.push_back(block);
  module.functions.push_back(func);

  EXPECT_FALSE(verifyMIR(module).ok);
}

TEST(MIRBasicTest, PrinterIncludesRequiredSections) {
  MIRModule module;
  MIRFunction func;
  func.sourceFuncId = FunctionId(0);
  func.name = "main";
  func.returnType = I32Type;
  func.parameterVRegs.push_back(VRegId(0));

  FrameObject home;
  home.kind = FrameObjectKind::VRegHome;
  home.vregId = VRegId(0);
  func.frameObjects.push_back(home);

  MIRBlock block;
  block.id = BlockId(0);
  block.label = ".Lbb.0";
  block.insts.push_back(MIRInstruction::make(MIROpcode::Return));
  func.blocks.push_back(block);
  module.functions.push_back(func);

  std::ostringstream out;
  dumpMIR(module, out);
  auto text = out.str();
  EXPECT_NE(text.find("func main"), std::string::npos);
  EXPECT_NE(text.find("vreg"), std::string::npos);
  EXPECT_NE(text.find("frame_objects"), std::string::npos);
  EXPECT_NE(text.find("ret"), std::string::npos);
}

static MIRFunction makeSingleReturnFunction(bool hasCall) {
  MIRFunction func;
  func.sourceFuncId = FunctionId(0);
  func.name = hasCall ? "caller" : "leaf";
  func.returnType = I32Type;
  func.hasCall = hasCall;
  auto v = func.allocVReg();

  FrameObject home;
  home.kind = FrameObjectKind::VRegHome;
  home.vregId = v;
  func.addFrameObject(home);

  if (hasCall) {
    FrameObject savedRa;
    savedRa.kind = FrameObjectKind::SavedReturnAddress;
    func.addFrameObject(savedRa);
  }

  MIRBlock block;
  block.id = BlockId(0);
  block.label = ".Lbb.0";
  block.insts.push_back(MIRInstruction::make(
      MIROpcode::LoadImm, {MIROperand::makeVReg(v), MIROperand::makeImm(0)}));
  block.insts.push_back(MIRInstruction::make(
      MIROpcode::Move, {MIROperand::makePhysReg(RV32PhysReg::A0), MIROperand::makeVReg(v)}));
  block.insts.push_back(MIRInstruction::make(MIROpcode::Return));
  func.blocks.push_back(block);
  return func;
}

TEST(MIRBasicTest, SpillAllAllocatorKeepsLeafWithoutSavedRa) {
  MIRModule module;
  module.functions.push_back(makeSingleReturnFunction(false));

  riscv32::SpillAllAllocator allocator;
  auto allocated = allocator.allocate(std::move(module));

  const auto& objects = allocated.functions.front().function.frameObjects;
  auto hasSavedRa = false;
  for (const auto& object : objects) {
    hasSavedRa = hasSavedRa || object.kind == FrameObjectKind::SavedReturnAddress;
  }
  EXPECT_FALSE(hasSavedRa);
}

TEST(MIRBasicTest, SpillAllAllocatorKeepsNonLeafSavedRa) {
  MIRModule module;
  module.functions.push_back(makeSingleReturnFunction(true));

  riscv32::SpillAllAllocator allocator;
  auto allocated = allocator.allocate(std::move(module));

  int savedRaCount = 0;
  for (const auto& object : allocated.functions.front().function.frameObjects) {
    if (object.kind == FrameObjectKind::SavedReturnAddress) ++savedRaCount;
  }
  EXPECT_EQ(savedRaCount, 1);
  EXPECT_EQ(allocated.functions.front().frameLayout.totalSize % 16, 0);
}

TEST(MIRBasicTest, TenArgumentCallGetsTwoOutgoingSlots) {
  MIRFunction func = makeSingleReturnFunction(true);
  func.maxOutgoingArgCount = 10;
  for (int i = 8; i < 10; ++i) {
    FrameObject outArg;
    outArg.kind = FrameObjectKind::OutgoingArgument;
    outArg.paramIndex = i;
    func.addFrameObject(outArg);
  }

  auto layout = FrameLayout::compute(func);

  EXPECT_EQ(layout.outgoingArgSize, 8);
  EXPECT_EQ(layout.outgoingArgOffset(8), 0);
  EXPECT_EQ(layout.outgoingArgOffset(9), 4);
}

TEST(MIRBasicTest, NinthIncomingArgumentUsesCallerOldSp) {
  MIRFunction func = makeSingleReturnFunction(false);
  auto layout = FrameLayout::compute(func);

  EXPECT_EQ(layout.incomingArgOffset(8), layout.totalSize);
}

} // namespace toyc
