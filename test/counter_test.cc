#include "../spectator/counter.h"
#include "../spectator/memory.h"
#include <gtest/gtest.h>

namespace {
std::unique_ptr<spectator::Counter> getCounter(std::string name) {
  auto id = std::make_shared<spectator::Id>(name, spectator::Tags{});
  return std::make_unique<spectator::Counter>(id);
}

TEST(Counter, Increment) {
  auto c = getCounter("foo");
  EXPECT_DOUBLE_EQ(c->Count(), 0.0);

  c->Increment();
  EXPECT_DOUBLE_EQ(c->Count(), 1.0);

  c->Increment();
  EXPECT_DOUBLE_EQ(c->Count(), 2.0);
}

TEST(Counter, Add) {
  auto c = getCounter("add");
  EXPECT_DOUBLE_EQ(c->Count(), 0.0);

  c->Add(42);
  EXPECT_DOUBLE_EQ(c->Count(), 42.0);

  c->Add(42);
  EXPECT_DOUBLE_EQ(c->Count(), 84.0);
}

TEST(Counter, AddFloat) {
  auto c = getCounter("add");
  EXPECT_DOUBLE_EQ(c->Count(), 0.0);

  c->Add(42.4);
  EXPECT_DOUBLE_EQ(c->Count(), 42.4);

  c->Add(0.6);
  EXPECT_DOUBLE_EQ(c->Count(), 43.0);
}

TEST(Counter, NegativeDelta) {
  auto c = getCounter("neg");
  EXPECT_DOUBLE_EQ(c->Count(), 0.0);

  c->Add(1.0);
  c->Add(-0.1);
  EXPECT_DOUBLE_EQ(c->Count(), 1.0);

  c->Add(-1.1);
  EXPECT_DOUBLE_EQ(c->Count(), 1.0);
}

TEST(Counter, Id) {
  auto c = getCounter("id");
  auto id = std::make_shared<spectator::Id>("id", spectator::Tags{});
  EXPECT_EQ(*(c->MeterId()), *id);
}

TEST(Counter, Measure) {
  auto c = getCounter("measure");
  c->Add(42);
  EXPECT_DOUBLE_EQ(c->Count(), 42.0);

  auto measures = c->Measure();
  EXPECT_DOUBLE_EQ(c->Count(), 0.0)
      << "Counters should reset their value after being measured";

  auto counter_id = c->MeterId()->WithStat("count");
  using spectator::Measurement;
  std::vector<Measurement> expected({{std::move(counter_id), 42.0}});
  EXPECT_EQ(expected, measures);

  EXPECT_TRUE(c->Measure().empty())
      << "Counters should not report 0 increments";
}

TEST(Counter, Updated) {
  auto counter = getCounter("c");
  counter->Increment();

  auto t1 = counter->Updated();
  auto t2 = counter->Updated();
  EXPECT_EQ(t1, t2);

  usleep(1);
  counter->Increment();
  EXPECT_TRUE(counter->Updated() > t1);
}
}  // namespace
