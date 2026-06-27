#include "toyc/driver/options.h"

#include <gtest/gtest.h>

namespace toyc {

TEST(DriverDumpSSATest, OptionParsing) {
  char arg0[] = "toycc";
  char arg1[] = "--dump-ssa";
  char* argv[] = {arg0, arg1};
  auto opts = CompilerOptions::parse(2, argv);
  EXPECT_TRUE(opts.dumpSsa);
  EXPECT_FALSE(opts.hasCommandLineError);
}

TEST(DriverDumpSSATest, DumpModesAreExclusive) {
  char arg0[] = "toycc";
  char arg1[] = "--dump-ir";
  char arg2[] = "--dump-ssa";
  char* argv[] = {arg0, arg1, arg2};
  auto opts = CompilerOptions::parse(3, argv);
  EXPECT_TRUE(opts.hasCommandLineError);
}

TEST(DriverDumpSSATest, OptWithDumpSSAParses) {
  char arg0[] = "toycc";
  char arg1[] = "-opt";
  char arg2[] = "--dump-ssa";
  char* argv[] = {arg0, arg1, arg2};
  auto opts = CompilerOptions::parse(3, argv);
  EXPECT_TRUE(opts.optimize);
  EXPECT_TRUE(opts.dumpSsa);
  EXPECT_FALSE(opts.hasCommandLineError);
}

} // namespace toyc
