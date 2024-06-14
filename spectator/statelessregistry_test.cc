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

TEST(StatelessRegistry, AgeGauge) {
  TestStatelessRegistry r;
  auto ag = r.GetAgeGauge("foo");
  auto ag2 = r.GetAgeGauge("bar", {{"id", "2"}});
  ag->Now();
  ag2->Set(100);
  std::vector<std::string> expected = {"A:foo:0", "A:bar,id=2:100"};
  EXPECT_EQ(r.SentMessages(), expected);
}

TEST(StatelessRegistry, Counter) {
  TestStatelessRegistry r;
  auto c = r.GetCounter("foo");
  c->Increment();
  EXPECT_EQ(r.SentMessages().front(), "c:foo:1");

  r.Reset();
  c = r.GetCounter("foo", {{"k1", "v1"}});
  c->Add(2);
  EXPECT_EQ(r.SentMessages().front(), "c:foo,k1=v1:2");
}

TEST(StatelessRegistry, DistSummary) {
  TestStatelessRegistry r;
  auto ds = r.GetDistributionSummary("foo");
  ds->Record(100);
  EXPECT_EQ(r.SentMessages().front(), "d:foo:100");

  r.Reset();
  ds = r.GetDistributionSummary("bar", {{"k1", "v1"}});
  ds->Record(2);
  EXPECT_EQ(r.SentMessages().front(), "d:bar,k1=v1:2");
}

TEST(StatelessRegistry, Gauge) {
  TestStatelessRegistry r;
  auto g = r.GetGauge("foo");
  auto g2 = r.GetGauge("bar", {{"id", "2"}});
  g->Set(100);
  g2->Set(101);
  std::vector<std::string> expected = {"g:foo:100", "g:bar,id=2:101"};
  EXPECT_EQ(r.SentMessages(), expected);
}

TEST(StatelessRegistry, MaxGauge) {
  TestStatelessRegistry r;
  auto m = r.GetMaxGauge("foo");
  auto m2 = r.GetMaxGauge("bar", {{"id", "2"}});
  m->Update(100);
  m2->Set(101);
  std::vector<std::string> expected = {"m:foo:100", "m:bar,id=2:101"};
  EXPECT_EQ(r.SentMessages(), expected);
}

TEST(StatelessRegistry, MonotonicCounter) {
  TestStatelessRegistry r;
  auto m = r.GetMonotonicCounter("foo");
  auto m2 = r.GetMonotonicCounter("bar", {{"id", "2"}});
  m->Set(101.1);
  m2->Set(102.2);
  std::vector<std::string> expected = {"C:foo:101.1", "C:bar,id=2:102.2"};
  EXPECT_EQ(r.SentMessages(), expected);
}

TEST(StatelessRegistry, MonotonicCounterUint) {
  TestStatelessRegistry r;
  auto m = r.GetMonotonicCounterUint("foo");
  auto m2 = r.GetMonotonicCounterUint("bar", {{"id", "2"}});
  m->Set(100);
  m2->Set(101);
  std::vector<std::string> expected = {"U:foo:100", "U:bar,id=2:101"};
  EXPECT_EQ(r.SentMessages(), expected);
}

TEST(StatelessRegistry, Timer) {
  TestStatelessRegistry r;
  auto t = r.GetTimer("foo");
  auto t2 = r.GetTimer("bar", {{"id", "2"}});
  t->Record(std::chrono::microseconds(100));
  t2->Record(absl::Seconds(0.1));
  std::vector<std::string> expected = {"t:foo:0.0001", "t:bar,id=2:0.1"};
  EXPECT_EQ(r.SentMessages(), expected);
}

TEST(StatelessRegistry, PercentileTimer) {
  TestStatelessRegistry r;
  auto t = r.GetPercentileTimer("foo", absl::ZeroDuration(), absl::Seconds(10));
  auto t2 = r.GetPercentileTimer("bar", absl::Milliseconds(1), absl::Seconds(1));

  t->Record(std::chrono::microseconds(100));
  t2->Record(std::chrono::microseconds(100));

  t->Record(absl::Seconds(5));
  t2->Record(absl::Seconds(5));

  t->Record(std::chrono::milliseconds(100));
  t2->Record(std::chrono::milliseconds(100));

  std::vector<std::string> expected = {"T:foo:0.0001", "T:bar:0.001",
                                       "T:foo:5",      "T:bar:1",
                                       "T:foo:0.1",    "T:bar:0.1"};
  EXPECT_EQ(r.SentMessages(), expected);
}

TEST(StatelessRegistry, PercentileDistributionSummary) {
  TestStatelessRegistry r;
  auto t = r.GetPercentileDistributionSummary("foo", 0, 1000);
  auto t2 = r.GetPercentileDistributionSummary("bar", 10, 100);

  t->Record(5);
  t2->Record(5);

  t->Record(500);
  t2->Record(500);

  t->Record(50);
  t2->Record(50);

  std::vector<std::string> expected = {"D:foo:5",   "D:bar:10",
                                       "D:foo:500", "D:bar:100",
                                       "D:foo:50",  "D:bar:50"};
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

  // DistSummaries
  auto d_name = r.GetDistributionSummary("ds");
  auto d_id = r.GetDistributionSummary(Id::of("ds"));
  test_meter(d_name, d_id);

  // Gauges
  auto g_name = r.GetGauge("g");
  auto g_id = r.GetGauge(Id::of("g"));
  test_meter(g_name, g_id);

  // MaxGauge
  auto mx_name = r.GetMaxGauge("m1");
  auto mx_id = r.GetMaxGauge(Id::of("m1"));
  test_meter(mx_name, mx_id);

  // MonoCounter
  auto mo_name = r.GetMonotonicCounter("mo1");
  auto mo_id = r.GetMonotonicCounter(Id::of("mo1"));
  test_meter(mo_name, mo_id);

  // MonoCounter Uint
  auto mo_u_name = r.GetMonotonicCounterUint("mo1");
  auto mo_u_id = r.GetMonotonicCounterUint(Id::of("mo1"));
  test_meter(mo_name, mo_id);

  // Pct DistSummaries
  auto pds_name = r.GetPercentileDistributionSummary("pds", 0, 100);
  auto pds_id = r.GetPercentileDistributionSummary(Id::of("pds"), 0, 100);
  test_meter(pds_name, pds_id);

  // Pct Timers
  auto pt_name =
      r.GetPercentileTimer("t", absl::ZeroDuration(), absl::Seconds(1));
  auto pt_id =
      r.GetPercentileTimer(Id::of("t"), absl::ZeroDuration(), absl::Seconds(1));
  test_meter(pt_name, pt_id);

  // Timers
  auto t_name = r.GetTimer("t1");
  auto t_id = r.GetTimer(Id::of("t1"));
  test_meter(t_name, t_id);
}

}  // namespace
