/// Frontend smoke tests — P0 verification (CompilerOptions only).
/// Token and Lexer tests are in token_test.cpp, lexer_test.cpp, lexer_error_test.cpp.

#include "toyc/driver/options.h"

#include <gtest/gtest.h>

namespace toyc {

TEST(CompilerOptionsTest, OptFlagParsed) {
  const char* argv[] = {"toycc", "-opt"};
  auto opts = CompilerOptions::parse(2, const_cast<char**>(argv));
  EXPECT_TRUE(opts.optimize);
  EXPECT_FALSE(opts.help);
  EXPECT_FALSE(opts.dumpTokens);
  EXPECT_FALSE(opts.hasCommandLineError);
}

TEST(CompilerOptionsTest, HelpFlagParsed) {
  const char* argv[] = {"toycc", "--help"};
  auto opts = CompilerOptions::parse(2, const_cast<char**>(argv));
  EXPECT_TRUE(opts.help);
  EXPECT_FALSE(opts.optimize);
}

TEST(CompilerOptionsTest, DefaultFlagsOff) {
  const char* argv[] = {"toycc"};
  auto opts = CompilerOptions::parse(1, const_cast<char**>(argv));
  EXPECT_FALSE(opts.help);
  EXPECT_FALSE(opts.optimize);
  EXPECT_FALSE(opts.verbose);
  EXPECT_FALSE(opts.dumpTokens);
  EXPECT_FALSE(opts.hasCommandLineError);
}

} // namespace toyc
