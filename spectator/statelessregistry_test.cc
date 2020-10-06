#include "registry.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {

using spectator::base_registry;
using spectator::stateless;
using spectator::stateless_types;
using spectator::TestPublisher;

// A stateless registry that uses a test publisher
class TestStatelessRegistry
    : public base_registry<stateless<stateless_types<TestPublisher>>> {
 public:
  TestStatelessRegistry() {
    state_.publisher = std::make_unique<TestPublisher>();
  }
  auto SentMessages() { return state_.publisher->SentMessages(); }
  void Reset() { return state_.publisher->Reset(); }
};

TEST(StatelessRegistry, Counter) {
  TestStatelessRegistry r;
  auto c = r.GetCounter("foo");
  c->Increment();
  EXPECT_EQ(r.SentMessages().front(), "1:c:foo:1");
  r.Reset();
  auto c2 = r.GetCounter("foo", {{"k1", "v1"}});
  c2->Add(2);
  EXPECT_EQ(r.SentMessages().front(), "1:c:foo:#k1=v1:2");
}

TEST(StatelessRegistry, DistSummary) {
  TestStatelessRegistry r;
  auto ds = r.GetDistributionSummary("ds");
  ds->Record(100);
  EXPECT_EQ(r.SentMessages().front(), "1:d:ds:100");
  r.Reset();
  auto c2 = r.GetDistributionSummary("ds", {{"k1", "v1"}});
  c2->Record(2);
  EXPECT_EQ(r.SentMessages().front(), "1:d:ds:#k1=v1:2");
}

TEST(StatelessRegistry, Gauge) {
  TestStatelessRegistry r;
  auto g = r.GetGauge("g");
  auto g2 = r.GetGauge("g", {{"id", "2"}});
  g->Set(100);
  g2->Set(101);
  std::vector<std::string> expected = {"1:g:g:100", "1:g:g:#id=2:101"};
  EXPECT_EQ(r.SentMessages(), expected);
}

TEST(StatelessRegistry, MaxGauge) {
  TestStatelessRegistry r;
  auto g = r.GetMaxGauge("g");
  auto g2 = r.GetMaxGauge("g", {{"id", "2"}});
  g->Update(100);
  g2->Set(101);
}

TEST(StatelessRegistry, MonotonicCounter) {
  TestStatelessRegistry r;
  auto c = r.GetMonotonicCounter("m");
  auto c2 = r.GetMonotonicCounter("m", {{"id", "2"}});
  c->Set(100);
}

TEST(StatelessRegistry, Timer) {
  TestStatelessRegistry r;
  auto t = r.GetTimer("t");
  auto t2 = r.GetTimer("t", {{"id", "2"}});
  t->Record(std::chrono::microseconds(100));
  t2->Record(absl::Seconds(0.1));
}

TEST(StatelessRegistry, PercentileTimer) {
  TestStatelessRegistry r;
  auto t =
      r.GetPercentileTimer("name", absl::ZeroDuration(), absl::Seconds(10));
  auto t2 =
      r.GetPercentileTimer("name2", absl::Milliseconds(1), absl::Seconds(1));

  t->Record(std::chrono::microseconds(100));
  t2->Record(std::chrono::microseconds(100));

  t->Record(absl::Seconds(5));
  t2->Record(absl::Seconds(5));

  t->Record(std::chrono::milliseconds(100));
  t2->Record(std::chrono::milliseconds(100));

  std::vector<std::string> expected = {"1:T:name:0.0001", "1:T:name2:0.001",
                                       "1:T:name:5",      "1:T:name2:1",
                                       "1:T:name:0.1",    "1:T:name2:0.1"};
  EXPECT_EQ(r.SentMessages(), expected);
}

TEST(StatelessRegistry, PercentileDistributionSummary) {
  TestStatelessRegistry r;
  auto t = r.GetPercentileDistributionSummary("name", 0, 1000);
  auto t2 = r.GetPercentileDistributionSummary("name2", 10, 100);

  t->Record(5);
  t2->Record(5);

  t->Record(500);
  t2->Record(500);

  t->Record(50);
  t2->Record(50);

  std::vector<std::string> expected = {"1:D:name:5",   "1:D:name2:10",
                                       "1:D:name:500", "1:D:name2:100",
                                       "1:D:name:50",  "1:D:name2:50"};
  EXPECT_EQ(r.SentMessages(), expected);
}

}  // namespace
