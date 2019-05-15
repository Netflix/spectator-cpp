#include <fmt/ostream.h>
#include "../spectator/percentile_timer.h"
#include <gtest/gtest.h>
#include "../spectator/memory.h"
#include "percentile_bucket_tags.inc"
#include "test_utils.h"

using namespace spectator;

using secs = std::chrono::seconds;
using nanos = std::chrono::nanoseconds;
using millis = std::chrono::milliseconds;

std::unique_ptr<PercentileTimer> getTimer(Registry* r) {
  auto id = r->CreateId("t", Tags{});
  return std::make_unique<PercentileTimer>(r, id, nanos{0}, secs{100});
}

TEST(PercentileTimer, Percentile) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto t = getTimer(&r);

  for (auto i = 0; i < 100000; ++i) {
    t->Record(millis{i});
  }

  for (auto i = 0; i <= 100; ++i) {
    auto expected = static_cast<double>(i);
    auto threshold = 0.15 * expected;
    EXPECT_NEAR(expected, t->Percentile(i), threshold);
  }
}

TEST(PercentileTimer, Measure) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto t = getTimer(&r);

  auto elapsed = millis{42};
  t->Record(elapsed);

  auto elapsed_nanos = std::chrono::duration_cast<nanos>(elapsed);

  auto ms = r.Measurements();
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

TEST(PercentileTimer, CountTotal) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto t = getTimer(&r);

  for (auto i = 0; i < 100; ++i) {
    t->Record(nanos(i));
  }

  EXPECT_EQ(t->Count(), 100);
  EXPECT_EQ(t->TotalTime(), 100 * 99 / 2);  // sum(1,n) = n * (n - 1) / 2
}

TEST(PercentileTimer, Restrict) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto id = r.CreateId("t", Tags{});
  PercentileTimer t{&r, id, millis(5), secs{2}};

  t.Record(millis(1));
  t.Record(secs(10));
  t.Record(millis(10));

  // not restricted here
  nanos total = millis(1) + secs(10) + millis(10);
  EXPECT_EQ(t.TotalTime(), total.count());
  EXPECT_EQ(t.Count(), 3);

  auto measurements = r.Measurements();
  auto actual = measurements_to_map(measurements);
  auto expected = std::map<std::string, double>{};

  // 5ms
  auto minPercTag = kTimerTags.at(PercentileBucketIndexOf(5l * 1000 * 1000));
  // 2s
  auto maxPercTag =
      kTimerTags.at(PercentileBucketIndexOf(2l * 1000 * 1000 * 1000));
  // 10ms --> not restricted
  auto percTag = kTimerTags.at(PercentileBucketIndexOf(10l * 1000 * 1000));

  expected[fmt::format("t|percentile={}|statistic=percentile", minPercTag)] = 1;
  expected[fmt::format("t|percentile={}|statistic=percentile", maxPercTag)] = 1;
  expected[fmt::format("t|percentile={}|statistic=percentile", percTag)] = 1;

  expected["t|statistic=count"] = 3;
  auto elapsed_secs = total.count() / 1e9;
  auto totalSq = (1e6 * 1e6 + 10e9 * 10e9 + 10e6 * 10e6) / 1e18;
  expected["t|statistic=max"] = 10;
  expected["t|statistic=totalTime"] = elapsed_secs;
  expected["t|statistic=totalOfSquares"] = totalSq;

  ASSERT_EQ(expected.size(), actual.size());
  for (const auto& expected_m : expected) {
    EXPECT_DOUBLE_EQ(expected_m.second, actual[expected_m.first])
        << expected_m.first;
  }
}
