#include "../spectator/gauge.h"
#include "../spectator/memory.h"
#include <gtest/gtest.h>

namespace {
using spectator::Gauge;
std::unique_ptr<Gauge> getGauge(std::string name) {
  auto id = std::make_shared<spectator::Id>(name, spectator::Tags{});
  return std::make_unique<Gauge>(id);
}

TEST(Gauge, Init) {
  auto g = getGauge("g");
  auto v = g->Get();
  EXPECT_TRUE(std::isnan(v));
}

TEST(Gauge, Set) {
  auto g = getGauge("g");
  g->Set(42.0);
  EXPECT_DOUBLE_EQ(42.0, g->Get());
}

TEST(Gauge, Measure) {
  auto g = getGauge("m");
  g->Set(1.0);
  auto ms = g->Measure();
  auto id = g->MeterId()->WithStat("gauge");

  using spectator::Measurement;
  auto expected = std::vector<Measurement>({{std::move(id), 1.0}});
  EXPECT_EQ(expected, ms);

  auto v = g->Get();
  // reset behavior
  EXPECT_TRUE(std::isnan(v)) << "Gauges should reset after being measured";
  // filter NaNs at the source
  EXPECT_TRUE(g->Measure().empty()) << "Gauges should not include NaNs";
}
}  // namespace
