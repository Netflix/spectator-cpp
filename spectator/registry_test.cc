#include "registry.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {
using spectator::Config;
using spectator::DefaultLogger;
using spectator::Registry;
using spectator::Tags;
using spectator::TestPublisher;

class TestRegistry : public Registry {
 public:
  TestRegistry()
      : Registry(Config{}, DefaultLogger(), std::make_unique<TestPublisher>()) {
  }
  TestPublisher* GetPublisher() {
    auto p = publisher_.get();
    return reinterpret_cast<TestPublisher*>(p);
  }
  std::vector<std::string> GetMeasurements() {
    return GetPublisher()->Measurements();
  }

  std::string FirstMeasurement() {
    return GetPublisher()->Measurements().front();
  }
};

TEST(Registry, Counter) {
  TestRegistry r;
  auto c = r.GetCounter("foo");
  c->Increment();
  EXPECT_EQ(r.FirstMeasurement(), "1:c:foo:1");
  r.GetPublisher()->Reset();
  auto c2 = r.GetCounter("foo", Tags{{"k1", "v1"}});
  c2->Add(2);
  EXPECT_EQ(r.FirstMeasurement(), "1:c:foo:#k1=v1:2");
}

TEST(Registry, DistSummary) {
  TestRegistry r;
  auto ds = r.GetDistributionSummary("ds");
  ds->Record(100);
  EXPECT_EQ(r.FirstMeasurement(), "1:d:ds:100");
  r.GetPublisher()->Reset();
  auto c2 = r.GetDistributionSummary("ds", Tags{{"k1", "v1"}});
  c2->Record(2);
  EXPECT_EQ(r.FirstMeasurement(), "1:d:ds:#k1=v1:2");
}

TEST(Registry, Gauge) {
  TestRegistry r;
  auto g = r.GetGauge("g");
  auto g2 = r.GetGauge("g", Tags{{"id", "2"}});
  g->Set(100);
  g2->Set(101);
  std::vector<std::string> expected = {"1:g:g:100", "1:g:g:#id=2:101"};
  EXPECT_EQ(r.GetMeasurements(), expected);
}

TEST(Registry, MaxGauge) {
  TestRegistry r;
  auto g = r.GetMaxGauge("g");
  auto g2 = r.GetMaxGauge("g", Tags{{"id", "2"}});
  g->Update(100);
  g2->Set(101);
}

TEST(Registry, MonotonicCounter) {
  TestRegistry r;
  auto c = r.GetMonotonicCounter("m");
  auto c2 = r.GetMonotonicCounter("m", Tags{{"id", "2"}});
  c->Set(100);
}

TEST(Registry, Timer) {
  TestRegistry r;
  auto t = r.GetTimer("t");
  auto t2 = r.GetTimer("t", Tags{{"id", "2"}});
  t->Record(std::chrono::microseconds(100));
  t2->Record(absl::Seconds(0.1));
}

TEST(Registry, PercentileTimer) {
  TestRegistry r;
  auto t = r.GetPercentileTimer(r.CreateId("name", Tags{}),
                                absl::ZeroDuration(), absl::Seconds(10));
  auto t2 = r.GetPercentileTimer(r.CreateId("name2", Tags{}),
                                 absl::Milliseconds(1), absl::Seconds(1));

  t->Record(std::chrono::microseconds(100));
  t2->Record(std::chrono::microseconds(100));

  t->Record(absl::Seconds(5));
  t2->Record(absl::Seconds(5));

  t->Record(std::chrono::milliseconds(100));
  t2->Record(std::chrono::milliseconds(100));

  std::vector<std::string> expected = {"1:T:name:0.0001", "1:T:name2:0.001",
                                       "1:T:name:5",      "1:T:name2:1",
                                       "1:T:name:0.1",    "1:T:name2:0.1"};
  EXPECT_EQ(r.GetMeasurements(), expected);
}

TEST(Registry, PercentileDistributionSummary) {
  TestRegistry r;
  auto t =
      r.GetPercentileDistributionSummary(r.CreateId("name", Tags{}), 0, 1000);
  auto t2 =
      r.GetPercentileDistributionSummary(r.CreateId("name2", Tags{}), 10, 100);

  t->Record(5);
  t2->Record(5);

  t->Record(500);
  t2->Record(500);

  t->Record(50);
  t2->Record(50);

  std::vector<std::string> expected = {"1:D:name:5",   "1:D:name2:10",
                                       "1:D:name:500", "1:D:name2:100",
                                       "1:D:name:50",  "1:D:name2:50"};
  EXPECT_EQ(r.GetMeasurements(), expected);
}

}  // namespace
