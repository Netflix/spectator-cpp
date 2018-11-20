#include "../spectator/monotonic_counter.h"
#include "../spectator/memory.h"
#include <gtest/gtest.h>

namespace {

std::unique_ptr<spectator::MonotonicCounter> getMonotonicCounter(
    std::string name) {
  auto id = std::make_shared<spectator::Id>(name, spectator::Tags{});
  return std::make_unique<spectator::MonotonicCounter>(id);
}

TEST(MonotonicCounter, Init) {
  auto c = getMonotonicCounter("foo");
  EXPECT_TRUE(std::isnan(c->Delta()));

  c->Set(42);
  EXPECT_TRUE(std::isnan(c->Delta()));

  ASSERT_TRUE(c->Measure().empty());

  c->Set(84);
  EXPECT_DOUBLE_EQ(c->Delta(), 42.0);
}

TEST(MonotonicCounter, NegativeDelta) {
  auto c = getMonotonicCounter("neg");
  EXPECT_TRUE(std::isnan(c->Delta()));

  c->Set(100.0);
  EXPECT_TRUE(c->Measure().empty());

  c->Set(99.0);
  EXPECT_TRUE(c->Measure().empty());

  c->Set(98.0);
  EXPECT_TRUE(c->Measure().empty());

  c->Set(100.0);
  auto ms = c->Measure();
  EXPECT_EQ(ms.size(), 1);
  auto expected = spectator::Measurement{c->MeterId()->WithStat("count"), 2.0};
  EXPECT_EQ(expected, ms.front());
}

TEST(MonotonicCounter, Id) {
  auto c = getMonotonicCounter("id");
  auto id = std::make_shared<spectator::Id>("id", spectator::Tags{});
  EXPECT_EQ(*(c->MeterId()), *id);
}

TEST(MonotonicCounter, Measure) {
  auto c = getMonotonicCounter("measure");
  c->Set(42);
  EXPECT_TRUE(c->Measure().empty());  // initialize

  EXPECT_DOUBLE_EQ(c->Delta(), 0.0)
      << "MonotonicCounters should reset their value after being measured";

  c->Set(84.0);
  auto measures = c->Measure();
  using spectator::Measurement;
  std::vector<Measurement> expected({{c->MeterId()->WithStat("count"), 42.0}});
  EXPECT_EQ(expected, measures);

  EXPECT_TRUE(c->Measure().empty())
      << "MonotonicCounters should not report delta=0";
}
}  // namespace
