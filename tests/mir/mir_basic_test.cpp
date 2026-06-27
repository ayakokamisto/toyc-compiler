#include "toyc/mir/mir.h"

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

} // namespace toyc
