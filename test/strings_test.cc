#include "../spectator/strings.h"
#include <gtest/gtest.h>

namespace {
TEST(Strings, IStartsWith) {
  using spectator::IStartsWith;
  std::string str = "The Quick Brown fOX ...";
  EXPECT_TRUE(IStartsWith(str, "The Quick"));
  EXPECT_FALSE(IStartsWith(str, " The Quick"));
  EXPECT_TRUE(IStartsWith(str, "tHE qUICK"));
  EXPECT_FALSE(IStartsWith("", "."));
}

TEST(Strings, TrimRight) {
  using spectator::TrimRight;
  std::string str = " foo bar baz \t\v\n";
  TrimRight(&str);
  EXPECT_EQ(str, " foo bar baz");
}
}  // namespace
