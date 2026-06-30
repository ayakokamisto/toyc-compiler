#include "toyc/target/riscv32/asm_emitter.h"
#include "toyc/mir/mir.h"
#include "toyc/target/riscv32/spill_all_allocator.h"

#include <gtest/gtest.h>
#include <regex>

namespace toyc {

static MIRModule makeReturnModule(int value) {
  MIRModule module;
  MIRFunction func;
  func.sourceFuncId = FunctionId(0);
  func.name = "main";
  func.returnType = I32Type;
  auto v = func.allocVReg();

  FrameObject home;
  home.kind = FrameObjectKind::VRegHome;
  home.vregId = v;
  func.addFrameObject(home);

  MIRBlock block;
  block.id = BlockId(0);
  block.label = ".Lbb.0";
  block.insts.push_back(MIRInstruction::make(
      MIROpcode::LoadImm, {MIROperand::makeVReg(v), MIROperand::makeImm(value)}));
  block.insts.push_back(MIRInstruction::make(
      MIROpcode::Move, {MIROperand::makePhysReg(RV32PhysReg::A0), MIROperand::makeVReg(v)}));
  block.insts.push_back(MIRInstruction::make(MIROpcode::Return));
  func.blocks.push_back(block);
  module.functions.push_back(func);
  return module;
}

static riscv32::AllocatedMachineModule allocate(MIRModule module) {
  riscv32::SpillAllAllocator allocator;
  return allocator.allocate(std::move(module));
}

TEST(RV32AssemblyEmitterTest, EmitsMainTextAndReturnWithoutEcall) {
  auto asmText = riscv32::emitAssembly(allocate(makeReturnModule(42)));
  EXPECT_NE(asmText.find(".section .text"), std::string::npos);
  EXPECT_NE(asmText.find(".globl main"), std::string::npos);
  EXPECT_NE(asmText.find("main:"), std::string::npos);
  EXPECT_NE(asmText.find("mv a0"), std::string::npos);
  EXPECT_NE(asmText.find("ret"), std::string::npos);
  EXPECT_EQ(asmText.find("ecall"), std::string::npos);
}

TEST(RV32AssemblyEmitterTest, OptimizedAssemblyForReturnConstAvoidsLoadRoundTrip) {
  auto asmText = riscv32::emitAssembly(allocate(makeReturnModule(42)), true);
  EXPECT_NE(asmText.find("li t2, 42"), std::string::npos);
  EXPECT_EQ(asmText.find("lw "), std::string::npos);
  EXPECT_NE(asmText.find("main.epilogue:"), std::string::npos);
}

TEST(RV32AssemblyEmitterTest, EmitsNoMExtensionOpcodeForHelperCalls) {
  MIRModule module = makeReturnModule(0);
  auto& func = module.functions.front();
  func.hasCall = true;
  auto& block = func.blocks.front();
  block.insts.insert(block.insts.begin(), MIRInstruction::make(MIROpcode::Call));
  block.insts.front().comment = ".Ltoyc.mul_i32";

  auto asmText = riscv32::emitAssembly(allocate(std::move(module)));
  std::regex illegalOpcode(R"((^|\n)\s*(mul|div|rem)\s)");
  EXPECT_NE(asmText.find(".Ltoyc.mul_i32:"), std::string::npos);
  EXPECT_FALSE(std::regex_search(asmText, illegalOpcode));
}

TEST(RV32AssemblyEmitterTest, UsesLargeOffsetMaterialization) {
  MIRModule module;
  MIRFunction func;
  func.sourceFuncId = FunctionId(0);
  func.name = "main";
  auto v = func.allocVReg();

  for (int i = 0; i < 600; ++i) {
    FrameObject object;
    object.kind = FrameObjectKind::IRSlot;
    object.irSlot = SlotId(static_cast<uint32_t>(i));
    func.addFrameObject(object);
  }
  FrameObject home;
  home.kind = FrameObjectKind::VRegHome;
  home.vregId = v;
  func.addFrameObject(home);

  MIRBlock block;
  block.id = BlockId(0);
  block.label = ".Lbb.0";
  block.insts.push_back(MIRInstruction::make(
      MIROpcode::LoadImm, {MIROperand::makeVReg(v), MIROperand::makeImm(1)}));
  block.insts.push_back(MIRInstruction::make(MIROpcode::Return));
  func.blocks.push_back(block);
  module.functions.push_back(func);

  auto asmText = riscv32::emitAssembly(allocate(std::move(module)));
  EXPECT_NE(asmText.find("li t3,"), std::string::npos);
  EXPECT_NE(asmText.find("add t3, sp, t3"), std::string::npos);
}

} // namespace toyc
