#include <fmt/ostream.h>
#include "../spectator/percentile_distribution_summary.h"
#include <gtest/gtest.h>
#include "../spectator/memory.h"
#include "percentile_bucket_tags.inc"
#include "test_utils.h"

using namespace spectator;

std::unique_ptr<PercentileDistributionSummary> getDS(Registry* r) {
  auto id = r->CreateId("ds", Tags{});
  return std::make_unique<PercentileDistributionSummary>(r, id);
}

TEST(PercentileDistributionSummary, Percentile) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto t = getDS(&r);

  for (auto i = 0; i < 100000; ++i) {
    t->Record(i);
  }

  for (auto i = 0; i <= 100; ++i) {
    auto expected = 1e3 * i;
    auto threshold = 0.15 * expected;
    EXPECT_NEAR(expected, t->Percentile(i), threshold);
  }
}

TEST(PercentileDistributionSummary, Measure) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto t = getDS(&r);

  t->Record(42);

  auto ms = r.Measurements();
  auto actual = measurements_to_map(ms);
  auto expected = std::map<std::string, double>{};

  auto percentileTag = kDistTags.at(PercentileBucketIndexOf(42));
  expected[fmt::format("ds|percentile={}|statistic=percentile",
                       percentileTag)] = 1;

  expected["ds|statistic=count"] = 1;
  expected["ds|statistic=max"] = 42;
  expected["ds|statistic=totalAmount"] = 42;
  expected["ds|statistic=totalOfSquares"] = 42 * 42;

  ASSERT_EQ(expected.size(), actual.size());
  for (const auto& expected_m : expected) {
    EXPECT_DOUBLE_EQ(expected_m.second, actual[expected_m.first]);
  }
}

TEST(PercentileDistributionSummary, CountTotal) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto t = getDS(&r);

  for (auto i = 0; i < 100; ++i) {
    t->Record(i);
  }

  EXPECT_EQ(t->Count(), 100);
  EXPECT_EQ(t->TotalAmount(), 100 * 99 / 2);  // sum(1,n) = n * (n - 1) / 2
}
