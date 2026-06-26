/// Driver --dump-ir tests — end-to-end CLI verification.

#include "toyc/driver/options.h"

#include <gtest/gtest.h>
#include <string>

namespace toyc {

TEST(DumpIRTest, OptionParsing) {
  {
    char arg0[] = "toycc";
    char arg1[] = "--dump-ir";
    char* argv[] = {arg0, arg1};
    auto opts = CompilerOptions::parse(2, argv);
    EXPECT_TRUE(opts.dumpIr);
    EXPECT_FALSE(opts.hasCommandLineError);
  }

  {
    char arg0[] = "toycc";
    char* argv[] = {arg0};
    auto opts = CompilerOptions::parse(1, argv);
    EXPECT_FALSE(opts.dumpIr);
  }
}

TEST(DumpIRTest, DumpIRWithOpt) {
  char arg0[] = "toycc";
  char arg1[] = "-opt";
  char arg2[] = "--dump-ir";
  char* argv[] = {arg0, arg1, arg2};
  auto opts = CompilerOptions::parse(3, argv);
  EXPECT_TRUE(opts.dumpIr);
  EXPECT_TRUE(opts.optimize);
  EXPECT_FALSE(opts.hasCommandLineError);
}

TEST(DumpIRTest, ConflictsWithDumpTokens) {
  char arg0[] = "toycc";
  char arg1[] = "--dump-tokens";
  char arg2[] = "--dump-ir";
  char* argv[] = {arg0, arg1, arg2};
  auto opts = CompilerOptions::parse(3, argv);
  EXPECT_TRUE(opts.hasCommandLineError);
}

TEST(DumpIRTest, ConflictsWithDumpAst) {
  char arg0[] = "toycc";
  char arg1[] = "--dump-ast";
  char arg2[] = "--dump-ir";
  char* argv[] = {arg0, arg1, arg2};
  auto opts = CompilerOptions::parse(3, argv);
  EXPECT_TRUE(opts.hasCommandLineError);
}

TEST(DumpIRTest, ConflictsWithDumpSema) {
  char arg0[] = "toycc";
  char arg1[] = "--dump-sema";
  char arg2[] = "--dump-ir";
  char* argv[] = {arg0, arg1, arg2};
  auto opts = CompilerOptions::parse(3, argv);
  EXPECT_TRUE(opts.hasCommandLineError);
}

} // namespace toyc
