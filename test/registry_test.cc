#include "../spectator/registry.h"
#include <fmt/ostream.h>
#include <gtest/gtest.h>

namespace {
using spectator::DefaultLogger;
using spectator::GetConfiguration;
using spectator::Registry;

TEST(Registry, Counter) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto c = r.GetCounter("foo");
  c->Increment();
  EXPECT_EQ(c->Count(), 1);
}

TEST(Registry, CounterGet) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto c = r.GetCounter("foo");
  c->Increment();
  EXPECT_EQ(r.GetCounter("foo")->Count(), 1);
}

TEST(Registry, DistSummary) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto ds = r.GetDistributionSummary("ds");
  ds->Record(100);
  EXPECT_EQ(r.GetDistributionSummary("ds")->TotalAmount(), 100);
}

TEST(Registry, Gauge) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto g = r.GetGauge("g");
  g->Set(100);
  EXPECT_DOUBLE_EQ(r.GetGauge("g")->Get(), 100);
}

TEST(Registry, MaxGauge) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto g = r.GetMaxGauge("g");
  g->Update(100);
  EXPECT_DOUBLE_EQ(r.GetMaxGauge("g")->Get(), 100);
}

TEST(Registry, MonotonicCounter) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto c = r.GetMonotonicCounter("m");
  c->Set(100);
  c->Measure();
  c->Set(200);
  EXPECT_DOUBLE_EQ(r.GetMonotonicCounter("m")->Delta(), 100);
}

TEST(Registry, Timer) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto t = r.GetTimer("t");
  t->Record(std::chrono::microseconds(1));
  EXPECT_EQ(r.GetTimer("t")->TotalTime(), 1000);
}

TEST(Registry, WrongType) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto t = r.GetTimer("meter");
  t->Record(std::chrono::nanoseconds(1));

  // this should log an error but still return a Gauge
  auto g = r.GetGauge("meter");
  g->Set(42);

  // the actual meter in the registry is the timer
  EXPECT_EQ(r.GetTimer("meter")->TotalTime(), 1);

  // the gauge is not persisted
  EXPECT_TRUE(std::isnan(r.GetGauge("meter")->Get()));
}

TEST(Registry, Meters) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto t = r.GetTimer("t");
  auto c = r.GetCounter("c");
  r.GetTimer("t")->Count();
  auto meters = r.Meters();
  ASSERT_EQ(meters.size(), 2);
}

TEST(Registry, MeasurementTest) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto c = r.GetCounter("c");
  c->Increment();
  auto m = c->Measure().front();
  // test to string
  auto str = fmt::format("{}", m);
  EXPECT_EQ(str, "Measurement{Id(c, [statistic->count]),1}");

  // test equals
  c->Increment();
  auto m2 = c->Measure().front();
  EXPECT_EQ(m, m2);
}

struct ExpRegistry : public Registry {
  explicit ExpRegistry(std::unique_ptr<spectator::Config> cfg)
      : Registry(std::move(cfg), DefaultLogger()) {}

  void expire() { removed_expired_meters(); }
};

TEST(Registry, Expiration) {
  auto cfg = GetConfiguration();
  cfg->expiration_frequency = std::chrono::milliseconds(1);
  cfg->meter_ttl = std::chrono::milliseconds(1);
  ExpRegistry r{std::move(cfg)};

  auto c = r.GetCounter("c");
  auto g = r.GetGauge("g");
  auto d = r.GetDistributionSummary("d");
  auto t = r.GetTimer("t");
  auto m = r.GetMaxGauge("m");
  ASSERT_EQ(r.Meters().size(), 5);

  usleep(2000);  // 2ms
  c->Increment();
  r.expire();

  ASSERT_EQ(r.Meters().size(), 1);
}

}  // namespace
