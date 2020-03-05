#include "../spectator/config.h"
#include <gtest/gtest.h>

TEST(Config, Constructor) {
  std::map<std::string, std::string> common_tags{};
  static constexpr auto kDefault = 0;

  // just make sure that our documented Config constructor works
  spectator::Config config{
      common_tags, kDefault, kDefault, kDefault,
      kDefault,    kDefault, kDefault, "http://example.org/api/v1/publish"};
}