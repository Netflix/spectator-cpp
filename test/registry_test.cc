#include "../spectator/registry.h"
#include "test_utils.h"
#include <fmt/ostream.h>
#include <gtest/gtest.h>

namespace {
using spectator::DefaultLogger;
using spectator::GetConfiguration;
using spectator::Registry;
using spectator::Tags;

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
  auto g2 = r.GetGauge("g", Tags{{"id", "2"}});
  g->Set(100);
  g2->Set(101);
  EXPECT_DOUBLE_EQ(r.GetGauge("g")->Get(), 100);
  EXPECT_DOUBLE_EQ(r.GetGauge("g", Tags{{"id", "2"}})->Get(), 101);
}

TEST(Registry, MaxGauge) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto g = r.GetMaxGauge("g");
  auto g2 = r.GetMaxGauge("g", Tags{{"id", "2"}});
  g->Update(100);
  g2->Set(101);
  EXPECT_DOUBLE_EQ(r.GetMaxGauge("g")->Get(), 100);
  EXPECT_DOUBLE_EQ(r.GetMaxGauge("g", Tags{{"id", "2"}})->Get(), 101);
}

TEST(Registry, MonotonicCounter) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto c = r.GetMonotonicCounter("m");
  auto c2 = r.GetMonotonicCounter("m", Tags{{"id", "2"}});
  c->Set(100);
  c->Measure();
  c->Set(200);
  c2->Set(100);
  c2->Measure();
  c2->Set(201);
  EXPECT_DOUBLE_EQ(r.GetMonotonicCounter("m")->Delta(), 100);
  EXPECT_DOUBLE_EQ(r.GetMonotonicCounter("m", Tags{{"id", "2"}})->Delta(), 101);
}

TEST(Registry, Timer) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto t = r.GetTimer("t");
  auto t2 = r.GetTimer("t", Tags{{"id", "2"}});
  t->Record(std::chrono::microseconds(1));
  t2->Record(std::chrono::microseconds(2));
  EXPECT_EQ(r.GetTimer("t")->TotalTime(), 1000);
  EXPECT_EQ(r.GetTimer("t", Tags{{"id", "2"}})->TotalTime(), 2000);
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
  auto meters = my_meters(r);
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
  ASSERT_EQ(my_meters(r).size(), 5);

  usleep(2000);  // 2ms
  c->Increment();
  r.expire();

  ASSERT_EQ(my_meters(r).size(), 1);
}

TEST(Registry, Size) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto base_number = r.Meters().size();
  EXPECT_EQ(r.Size(), base_number);

  r.GetCounter("foo");
  r.GetTimer("bar");
  EXPECT_EQ(r.Size(), 2 + base_number);

  r.GetCounter("foo");
  r.GetTimer("bar2");
  EXPECT_EQ(r.Size(), 3 + base_number);
}

TEST(Registry, OnMeasurements) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto called = false;
  auto found = false;
  auto cb = [&](const std::vector<spectator::Measurement>& ms) {
    called = true;
    auto id =
        r.CreateId("some.counter", spectator::Tags{{"statistic", "count"}});
    auto expected = spectator::Measurement{std::move(id), 1.0};
    auto it = std::find(ms.begin(), ms.end(), expected);
    found = it != ms.end();
  };
  r.OnMeasurements(cb);
  r.GetCounter("some.counter")->Increment();
  r.Measurements();
  ASSERT_TRUE(called);
  ASSERT_TRUE(found);
}

TEST(Registry, DistSummary_Size) {
  Registry r{GetConfiguration(), DefaultLogger()};
  r.GetTimer("foo")->Record(std::chrono::seconds{42});
  r.GetCounter("bar")->Increment();
  // we have 4 measurements from the timer + 1 from the counter
  r.Measurements();
  EXPECT_DOUBLE_EQ(
      r.GetDistributionSummary("spectator.registrySize")->TotalAmount(), 5.0);
}

}  // namespace
