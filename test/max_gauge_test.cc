#include "../spectator/max_gauge.h"
#include "../spectator/memory.h"
#include <gtest/gtest.h>

namespace {

using spectator::Measurement;
std::unique_ptr<spectator::MaxGauge> getMaxGauge(std::string name) {
  auto id = std::make_shared<spectator::Id>(name, spectator::Tags{});
  return std::make_unique<spectator::MaxGauge>(id);
}

TEST(MaxGauge, Init) {
  auto g = getMaxGauge("foo");
  EXPECT_TRUE(std::isnan(g->Get()));

  g->Set(42);
  EXPECT_DOUBLE_EQ(42.0, g->Get());

  auto ms = g->Measure();
  ASSERT_EQ(ms.size(), 1);
  auto expected = Measurement{g->MeterId()->WithStat("max"), 42.0};
  EXPECT_EQ(expected, ms.front());
}

TEST(MaxGauge, MaxUpdates) {
  auto g = getMaxGauge("max");
  g->Update(100.0);
  EXPECT_DOUBLE_EQ(g->Get(), 100.0);
  g->Update(101.0);
  EXPECT_DOUBLE_EQ(g->Get(), 101.0);
  g->Update(100.0);
  EXPECT_DOUBLE_EQ(g->Get(), 101.0);
}

TEST(MaxGauge, Id) {
  auto g = getMaxGauge("id");
  auto id = std::make_shared<spectator::Id>("id", spectator::Tags{});
  EXPECT_EQ(*(g->MeterId()), *id);
}

TEST(MaxGauge, Measure) {
  auto g = getMaxGauge("measure");
  EXPECT_TRUE(g->Measure().empty());  // initialize

  g->Update(42);
  auto ms = g->Measure();
  ASSERT_EQ(ms.size(), 1);
  std::vector<Measurement> expected({{g->MeterId()->WithStat("max"), 42.0}});
  EXPECT_EQ(expected, ms);
  EXPECT_TRUE(g->Measure().empty());  // reset after measurement

  g->Update(4.0);
  auto measures = g->Measure();
  expected.at(0).value = 4.0;
  EXPECT_EQ(expected, measures);
}

TEST(MaxGauge, Updated) {
  auto m = getMaxGauge("m");
  m->Update(1);

  auto t1 = m->Updated();
  auto t2 = m->Updated();
  EXPECT_EQ(t1, t2);

  usleep(1);
  m->Set(2);
  EXPECT_TRUE(m->Updated() > t1);
}

}  // namespace
