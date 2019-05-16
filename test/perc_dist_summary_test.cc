#include <fmt/ostream.h>
#include "../spectator/percentile_distribution_summary.h"
#include <gtest/gtest.h>
#include "../spectator/memory.h"
#include "percentile_bucket_tags.inc"
#include "test_utils.h"

using namespace spectator;

std::unique_ptr<PercentileDistributionSummary> getDS(Registry* r) {
  auto id = r->CreateId("ds", Tags{});
  return std::make_unique<PercentileDistributionSummary>(r, id, 0, 1000 * 1000);
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

TEST(PercentileDistributionSummary, Restrict) {
  Registry r{GetConfiguration(), DefaultLogger()};
  auto id = r.CreateId("ds", Tags{});
  PercentileDistributionSummary ds{&r, id, 5, 2000};

  ds.Record(-1);
  ds.Record(0);
  ds.Record(10);
  ds.Record(10000);

  auto total = 0 + 10 + 10000;  // we ignore the negative value
  EXPECT_EQ(ds.TotalAmount(), total);
  EXPECT_EQ(ds.Count(), 3);

  auto measurements = r.Measurements();
  auto actual = measurements_to_map(measurements);
  auto expected = std::map<std::string, double>{};

  auto minPercTag = kDistTags.at(PercentileBucketIndexOf(5));
  auto maxPercTag = kDistTags.at(PercentileBucketIndexOf(2000));
  auto percTag = kDistTags.at(PercentileBucketIndexOf(10));

  expected[fmt::format("ds|percentile={}|statistic=percentile", minPercTag)] =
      1;
  expected[fmt::format("ds|percentile={}|statistic=percentile", maxPercTag)] =
      1;
  expected[fmt::format("ds|percentile={}|statistic=percentile", percTag)] = 1;

  expected["ds|statistic=count"] = 3;
  auto totalSq = 10 * 10 + 10000 * 10000;
  expected["ds|statistic=max"] = 10000;
  expected["ds|statistic=totalAmount"] = total;
  expected["ds|statistic=totalOfSquares"] = totalSq;

  ASSERT_EQ(expected.size(), actual.size());
  for (const auto& expected_m : expected) {
    EXPECT_DOUBLE_EQ(expected_m.second, actual[expected_m.first])
        << expected_m.first;
  }
}
