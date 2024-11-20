#include "../spectator/meter_type.h"
#include <gtest/gtest.h>

namespace {

using spectator::MeterType;

TEST(MeterType, Format) {
  EXPECT_EQ(fmt::format("{}", MeterType::Counter), "counter");
}
}  // namespace
