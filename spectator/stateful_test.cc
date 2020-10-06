#include "registry.h"
#include <gtest/gtest.h>

namespace {

TEST(Stateful, Counter) {
  spectator::TestRegistry testRegistry;

  auto ctr = testRegistry.GetCounter(
      std::make_shared<spectator::Id>("foo", spectator::Tags()));
  ctr->Increment();

  EXPECT_EQ(testRegistry.Measurements().size(), 1);
  EXPECT_EQ(testRegistry.Measurements().size(), 0);
}

TEST(Stateful, Timer) {
  spectator::TestRegistry testRegistry;
  testRegistry.GetTimer("name")->Record(absl::Seconds(0.5));
  EXPECT_EQ(testRegistry.Measurements().size(), 4);
}

TEST(Stateful, Gauge) {
  spectator::TestRegistry testRegistry;
  testRegistry.GetGauge("name")->Set(1);
  EXPECT_EQ(testRegistry.Measurements().size(), 1);
}

TEST(Stateful, SameMeter) {
  spectator::TestRegistry registry;
  registry.GetCounter("foo")->Add(2);
  EXPECT_EQ(registry.GetCounter("foo")->Count(), 2);
}

}  // namespace