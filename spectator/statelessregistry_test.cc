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
  void AddExtraTag(absl::string_view k, absl::string_view v) {
    extra_tags_.add(k, v);
  }
};

TEST(StatelessRegistry, Counter) {
  TestStatelessRegistry r;
  auto c = r.GetCounter("foo");
  c->Increment();
  EXPECT_EQ(r.SentMessages().front(), "c:foo:1");
  r.Reset();
  auto c2 = r.GetCounter("foo", {{"k1", "v1"}});
  c2->Add(2);
  EXPECT_EQ(r.SentMessages().front(), "c:foo,k1=v1:2");
}

TEST(StatelessRegistry, DistSummary) {
  TestStatelessRegistry r;
  auto ds = r.GetDistributionSummary("ds");
  ds->Record(100);
  EXPECT_EQ(r.SentMessages().front(), "d:ds:100");
  r.Reset();
  auto c2 = r.GetDistributionSummary("ds", {{"k1", "v1"}});
  c2->Record(2);
  EXPECT_EQ(r.SentMessages().front(), "d:ds,k1=v1:2");
}

TEST(StatelessRegistry, Gauge) {
  TestStatelessRegistry r;
  auto g = r.GetGauge("g");
  auto g2 = r.GetGauge("g", {{"id", "2"}});
  g->Set(100);
  g2->Set(101);
  std::vector<std::string> expected = {"g:g:100", "g:g,id=2:101"};
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

  std::vector<std::string> expected = {"T:name:0.0001", "T:name2:0.001",
                                       "T:name:5",      "T:name2:1",
                                       "T:name:0.1",    "T:name2:0.1"};
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

  std::vector<std::string> expected = {"D:name:5",   "D:name2:10",
                                       "D:name:500", "D:name2:100",
                                       "D:name:50",  "D:name2:50"};
  EXPECT_EQ(r.SentMessages(), expected);
}

template <typename T>
void test_meter(T&& m1, T&& m2) {
  auto id1 = m1->MeterId();
  auto id2 = m2->MeterId();
  EXPECT_EQ(*id1, *id2);
  spectator::Tags expected{{"x.spectator", "v1"}};
  EXPECT_EQ(id1->GetTags(), expected);
}

TEST(StatelessRegistry, ExtraTags) {
  using spectator::Id;

  TestStatelessRegistry r;
  r.AddExtraTag("x.spectator", "v1");

  // Counters
  auto c_name = r.GetCounter("name");
  auto c_id = r.GetCounter(Id::of("name"));
  test_meter(c_name, c_id);

  // DistSum
  auto d_name = r.GetDistributionSummary("ds");
  auto d_id = r.GetDistributionSummary(Id::of("ds"));
  test_meter(d_name, d_id);

  // Gauges
  auto g_name = r.GetGauge("g");
  auto g_id = r.GetGauge(Id::of("g"));
  test_meter(g_name, g_id);

  // Timers
  auto t_name = r.GetTimer("t1");
  auto t_id = r.GetTimer(Id::of("t1"));
  test_meter(t_name, t_id);

  // MaxGauge
  auto mx_name = r.GetMaxGauge("m1");
  auto mx_id = r.GetMaxGauge(Id::of("m1"));
  test_meter(mx_name, mx_id);

  // MonoCounter
  auto mo_name = r.GetMonotonicCounter("mo1");
  auto mo_id = r.GetMonotonicCounter(Id::of("mo1"));
  test_meter(mo_name, mo_id);

  // PercDs
  auto pds_name = r.GetPercentileDistributionSummary("pds", 0, 100);
  auto pds_id = r.GetPercentileDistributionSummary(Id::of("pds"), 0, 100);
  test_meter(pds_name, pds_id);

  auto pt_name =
      r.GetPercentileTimer("t", absl::ZeroDuration(), absl::Seconds(1));
  auto pt_id =
      r.GetPercentileTimer(Id::of("t"), absl::ZeroDuration(), absl::Seconds(1));
  test_meter(pt_name, pt_id);
}

}  // namespace
