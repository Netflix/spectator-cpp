#include <gtest/gtest.h>
#include "../spectator/percentile_timer.h"
#include "../spectator/memory.h"
#include "percentile_bucket_tags.inc"
#include "test_utils.h"
#include <fmt/ostream.h>

namespace {
using namespace spectator;

using secs = std::chrono::seconds;
using nanos = std::chrono::nanoseconds;
using millis = std::chrono::milliseconds;

template <class T>
std::unique_ptr<T> getTimer(Registry* r) {
  auto id = r->CreateId("t", Tags{});
  return std::make_unique<T>(r, id, nanos{0}, secs{100});
}

using Implementations = testing::Types<PercentileTimer, EagerPercentileTimer>;

template <class T>
class PercentileTimerTest : public ::testing::Test {
 protected:
  PercentileTimerTest()
      : r{GetConfiguration(), DefaultLogger()},
        timer{getTimer<T>(&r)},
        restricted_timer{&r, r.CreateId("t2", Tags{}), millis(5), secs(2)} {}
  Registry r;
  std::unique_ptr<T> timer;
  T restricted_timer;
};

TYPED_TEST_SUITE(PercentileTimerTest, Implementations);

TYPED_TEST(PercentileTimerTest, Percentile) {
  for (auto i = 0; i < 100000; ++i) {
    this->timer->Record(millis{i});
  }

  for (auto i = 0; i <= 100; ++i) {
    auto expected = static_cast<double>(i);
    auto threshold = 0.15 * expected;
    EXPECT_NEAR(expected, this->timer->Percentile(i), threshold);
  }
}

TYPED_TEST(PercentileTimerTest, Measure) {
  auto elapsed = millis{42};
  this->timer->Record(elapsed);

  auto elapsed_nanos = std::chrono::duration_cast<nanos>(elapsed);

  auto ms = this->r.Measurements();
  auto actual = measurements_to_map(ms);
  auto expected = std::map<std::string, double>{};

  auto percentileTag =
      kTimerTags.at(PercentileBucketIndexOf(elapsed_nanos.count()));
  expected[fmt::format("t|percentile={}|statistic=percentile", percentileTag)] =
      1;

  expected["t|statistic=count"] = 1;
  auto elapsed_secs = elapsed_nanos.count() / 1e9;
  expected["t|statistic=max"] = elapsed_secs;
  expected["t|statistic=totalTime"] = elapsed_secs;
  expected["t|statistic=totalOfSquares"] = elapsed_secs * elapsed_secs;

  ASSERT_EQ(expected.size(), actual.size());
  for (const auto& expected_m : expected) {
    EXPECT_DOUBLE_EQ(expected_m.second, actual[expected_m.first])
        << expected_m.first;
  }
}

TYPED_TEST(PercentileTimerTest, CountTotal) {
  for (auto i = 0; i < 100; ++i) {
    this->timer->Record(nanos(i));
  }

  EXPECT_EQ(this->timer->Count(), 100);
  EXPECT_EQ(this->timer->TotalTime(),
            100 * 99 / 2);  // sum(1,n) = n * (n - 1) / 2
}

TYPED_TEST(PercentileTimerTest, Restrict) {
  auto& t = this->restricted_timer;

  t.Record(millis(1));
  t.Record(secs(10));
  t.Record(millis(10));

  // not restricted here
  nanos total = millis(1) + secs(10) + millis(10);
  EXPECT_EQ(t.TotalTime(), total.count());
  EXPECT_EQ(t.Count(), 3);

  auto measurements = this->r.Measurements();
  auto actual = measurements_to_map(measurements);
  auto expected = std::map<std::string, double>{};

  // 5ms
  auto minPercTag = kTimerTags.at(PercentileBucketIndexOf(5l * 1000 * 1000));
  // 2s
  auto maxPercTag =
      kTimerTags.at(PercentileBucketIndexOf(2l * 1000 * 1000 * 1000));
  // 10ms --> not restricted
  auto percTag = kTimerTags.at(PercentileBucketIndexOf(10l * 1000 * 1000));
  auto name = t.MeterId()->Name();

  expected[fmt::format("{}|percentile={}|statistic=percentile", name,
                       minPercTag)] = 1;
  expected[fmt::format("{}|percentile={}|statistic=percentile", name,
                       maxPercTag)] = 1;
  expected[fmt::format("{}|percentile={}|statistic=percentile", name,
                       percTag)] = 1;

  expected[fmt::format("{}|statistic=count", name)] = 3;
  auto elapsed_secs = total.count() / 1e9;
  auto totalSq = (1e6 * 1e6 + 10e9 * 10e9 + 10e6 * 10e6) / 1e18;
  expected[fmt::format("{}|statistic=max", name)] = 10;
  expected[fmt::format("{}|statistic=totalTime", name)] = elapsed_secs;
  expected[fmt::format("{}|statistic=totalOfSquares", name)] = totalSq;

  ASSERT_EQ(expected.size(), actual.size());
  for (const auto& expected_m : expected) {
    EXPECT_DOUBLE_EQ(expected_m.second, actual[expected_m.first])
        << expected_m.first;
  }
}
}  // namespace
