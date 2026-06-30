#include "toyc/mir/mir_dead_writeback.h"

#include <gtest/gtest.h>

namespace toyc {

namespace {

int addVRegHome(MIRFunction& function, VRegId vreg) {
  FrameObject object;
  object.kind = FrameObjectKind::VRegHome;
  object.vregId = vreg;
  return function.addFrameObject(object);
}

MIRInstruction loadImm(VRegId dst, int value) {
  return MIRInstruction::make(MIROpcode::LoadImm,
                              {MIROperand::makeVReg(dst), MIROperand::makeImm(value)});
}

MIRInstruction storeFrame(int slot, VRegId src) {
  return MIRInstruction::make(MIROpcode::StoreFrame,
                              {MIROperand::makeFrameSlot(slot), MIROperand::makeVReg(src)});
}

MIRInstruction loadFrame(VRegId dst, int slot) {
  return MIRInstruction::make(MIROpcode::LoadFrame,
                              {MIROperand::makeVReg(dst), MIROperand::makeFrameSlot(slot)});
}

MIRFunction makeFunctionWithOneBlock() {
  MIRFunction function;
  function.sourceFuncId = FunctionId(0);
  function.name = "main";
  function.returnType = I32Type;
  MIRBlock block;
  block.id = BlockId(0);
  block.label = ".Lmain.entry";
  function.blocks.push_back(block);
  return function;
}

int countStoreFrameToSlot(const MIRFunction& function, int slot) {
  int count = 0;
  for (const auto& block : function.blocks) {
    for (const auto& inst : block.insts) {
      if (inst.opcode == MIROpcode::StoreFrame && inst.operands.size() >= 2 &&
          inst.operands[0].kind == MIROperandKind::FrameSlot &&
          inst.operands[0].frameSlotIndex() == slot) {
        ++count;
      }
    }
  }
  return count;
}

} // namespace

TEST(MIRDeadWritebackTest, StoreThenOverwriteSameBlock) {
  auto function = makeFunctionWithOneBlock();
  auto first = function.allocVReg();
  auto second = function.allocVReg();
  int slot = addVRegHome(function, first);
  function.blocks[0].insts = {
      loadImm(first, 1),
      storeFrame(slot, first),
      loadImm(second, 2),
      storeFrame(slot, second),
      MIRInstruction::make(MIROpcode::Return),
  };

  BlockLocalDeadWritebackStats stats;
  EXPECT_TRUE(eliminateBlockLocalDeadWritebacks(function, &stats));
  EXPECT_EQ(countStoreFrameToSlot(function, slot), 1);
  ASSERT_EQ(stats.deadWritebacksRemoved, 1);
  EXPECT_EQ(stats.removals.front().frameSlot, slot);
}

TEST(MIRDeadWritebackTest, StoreThenLoadSameBlock) {
  auto function = makeFunctionWithOneBlock();
  auto first = function.allocVReg();
  auto loaded = function.allocVReg();
  auto second = function.allocVReg();
  int slot = addVRegHome(function, first);
  function.blocks[0].insts = {
      loadImm(first, 1),
      storeFrame(slot, first),
      loadFrame(loaded, slot),
      loadImm(second, 2),
      storeFrame(slot, second),
      MIRInstruction::make(MIROpcode::Return),
  };

  BlockLocalDeadWritebackStats stats;
  (void)eliminateBlockLocalDeadWritebacks(function, &stats);
  EXPECT_EQ(countStoreFrameToSlot(function, slot), 2);
  EXPECT_EQ(stats.deadWritebacksRemoved, 0);
}

TEST(MIRDeadWritebackTest, StoreThenCall) {
  auto function = makeFunctionWithOneBlock();
  auto first = function.allocVReg();
  auto second = function.allocVReg();
  int slot = addVRegHome(function, first);
  function.blocks[0].insts = {
      loadImm(first, 1),
      storeFrame(slot, first),
      MIRInstruction::make(MIROpcode::Call, {MIROperand::makeGlobal(GlobalId(0))}),
      loadImm(second, 2),
      storeFrame(slot, second),
      MIRInstruction::make(MIROpcode::Return),
  };

  BlockLocalDeadWritebackStats stats;
  (void)eliminateBlockLocalDeadWritebacks(function, &stats);
  EXPECT_EQ(countStoreFrameToSlot(function, slot), 2);
  EXPECT_EQ(stats.deadWritebacksRemoved, 0);
}

TEST(MIRDeadWritebackTest, StoreBeforeBranch) {
  auto function = makeFunctionWithOneBlock();
  auto first = function.allocVReg();
  auto second = function.allocVReg();
  int slot = addVRegHome(function, first);
  function.blocks[0].insts = {
      loadImm(first, 1),
      storeFrame(slot, first),
      MIRInstruction::make(MIROpcode::BranchIfNonZero,
                           {MIROperand::makeVReg(first), MIROperand::makeBlockLabel(BlockId(1))}),
      loadImm(second, 2),
      storeFrame(slot, second),
      MIRInstruction::make(MIROpcode::Return),
  };

  BlockLocalDeadWritebackStats stats;
  (void)eliminateBlockLocalDeadWritebacks(function, &stats);
  EXPECT_EQ(countStoreFrameToSlot(function, slot), 2);
  EXPECT_EQ(stats.deadWritebacksRemoved, 0);
}

TEST(MIRDeadWritebackTest, StoreBeforeReturn) {
  auto function = makeFunctionWithOneBlock();
  auto first = function.allocVReg();
  int slot = addVRegHome(function, first);
  function.blocks[0].insts = {
      loadImm(first, 1),
      storeFrame(slot, first),
      MIRInstruction::make(MIROpcode::Return),
  };

  BlockLocalDeadWritebackStats stats;
  (void)eliminateBlockLocalDeadWritebacks(function, &stats);
  EXPECT_EQ(countStoreFrameToSlot(function, slot), 1);
  EXPECT_EQ(stats.deadWritebacksRemoved, 0);
}

TEST(MIRDeadWritebackTest, DifferentSlots) {
  auto function = makeFunctionWithOneBlock();
  auto first = function.allocVReg();
  auto second = function.allocVReg();
  int firstSlot = addVRegHome(function, first);
  int secondSlot = addVRegHome(function, second);
  function.blocks[0].insts = {
      loadImm(first, 1),
      storeFrame(firstSlot, first),
      loadImm(second, 2),
      storeFrame(secondSlot, second),
      MIRInstruction::make(MIROpcode::Return),
  };

  BlockLocalDeadWritebackStats stats;
  (void)eliminateBlockLocalDeadWritebacks(function, &stats);
  EXPECT_EQ(countStoreFrameToSlot(function, firstSlot), 1);
  EXPECT_EQ(countStoreFrameToSlot(function, secondSlot), 1);
  EXPECT_EQ(stats.deadWritebacksRemoved, 0);
}

TEST(MIRDeadWritebackTest, LoopBodyStoreKeepsBackedgeBoundaryStore) {
  MIRFunction function;
  function.sourceFuncId = FunctionId(0);
  function.name = "main";
  function.returnType = I32Type;
  auto first = function.allocVReg();
  auto second = function.allocVReg();
  int slot = addVRegHome(function, first);

  MIRBlock body;
  body.id = BlockId(1);
  body.label = ".Lloop.body";
  body.insts = {
      loadImm(first, 1),
      storeFrame(slot, first),
      loadImm(second, 2),
      storeFrame(slot, second),
      MIRInstruction::make(MIROpcode::Branch, {MIROperand::makeBlockLabel(BlockId(1))}),
  };
  function.blocks.push_back(body);

  BlockLocalDeadWritebackStats stats;
  EXPECT_TRUE(eliminateBlockLocalDeadWritebacks(function, &stats));
  EXPECT_EQ(countStoreFrameToSlot(function, slot), 1);
  ASSERT_EQ(stats.deadWritebacksRemoved, 1);
  EXPECT_EQ(function.blocks[0].insts.back().opcode, MIROpcode::Branch);
}

TEST(MIRDeadWritebackTest, MarksBlockLocalVRegHomeWritebackSuppression) {
  auto function = makeFunctionWithOneBlock();
  auto first = function.allocVReg();
  auto second = function.allocVReg();
  function.blocks[0].insts = {
      loadImm(first, 1),
      MIRInstruction::make(MIROpcode::Addi,
                           {MIROperand::makeVReg(second), MIROperand::makeVReg(first),
                            MIROperand::makeImm(2)}),
      MIRInstruction::make(MIROpcode::Move,
                           {MIROperand::makePhysReg(RV32PhysReg::A0),
                            MIROperand::makeVReg(second)}),
      MIRInstruction::make(MIROpcode::Return),
  };

  BlockLocalDeadWritebackStats stats;
  EXPECT_TRUE(eliminateBlockLocalDeadWritebacks(function, &stats));
  EXPECT_TRUE(function.blocks[0].insts[0].suppressVRegHomeStore);
  EXPECT_TRUE(function.blocks[0].insts[1].suppressVRegHomeStore);
  EXPECT_EQ(stats.vregHomeWritebacksSuppressed, 2);
}

} // namespace toyc
