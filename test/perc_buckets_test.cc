#include "../spectator/percentile_buckets.h"
#include <gtest/gtest.h>
#include <random>

using namespace spectator;

TEST(PercentileBuckets, IndexOf) {
  EXPECT_EQ(0, PercentileBucketIndexOf(-1));
  EXPECT_EQ(0, PercentileBucketIndexOf(0));
  EXPECT_EQ(1, PercentileBucketIndexOf(1));
  EXPECT_EQ(2, PercentileBucketIndexOf(2));
  EXPECT_EQ(3, PercentileBucketIndexOf(3));
  EXPECT_EQ(4, PercentileBucketIndexOf(4));
  EXPECT_EQ(25, PercentileBucketIndexOf(87));
  EXPECT_EQ(PercentileBucketsLength() - 1,
            PercentileBucketIndexOf(std::numeric_limits<int64_t>::max()));
}

TEST(PercentileBuckets, IndexOfSanityCheck) {
  std::mt19937_64 rng;
  rng.seed(std::random_device()());

  for (auto i = 0; i < 10000; ++i) {
    auto v = static_cast<int64_t>(rng());
    if (v < 0) {
      EXPECT_EQ(0, PercentileBucketIndexOf(v));
    } else {
      auto b = PercentileBucket(v);
      EXPECT_TRUE(v <= b) << v << " > " << b;
    }
  }
}

TEST(PercentileBuckets, BucketSanityCheck) {
  std::mt19937_64 rng;
  rng.seed(std::random_device()());

  for (auto i = 0; i < 10000; ++i) {
    auto v = static_cast<int64_t>(rng());
    if (v < 0) {
      EXPECT_EQ(1, PercentileBucket(v));
    } else {
      auto b = PercentileBucket(v);
      EXPECT_TRUE(v <= b) << v << " > " << b;
    }
  }
}

TEST(PercentileBuckets, Percentiles) {
  std::array<int64_t, PercentileBucketsLength()> counts{};

  for (int i = 0; i < 100000; ++i) {
    ++counts[PercentileBucketIndexOf(i)];
  }

  std::vector<double> pcts{0.0,  25.0, 50.0, 75.0, 90.0,
                           95.0, 98.0, 99.0, 99.5, 100.0};
  std::vector<double> results;
  Percentiles(counts, pcts, &results);
  ASSERT_EQ(results.size(), pcts.size());

  std::vector<double> expected = {0.0,  25e3, 50e3, 75e3,   90e3,
                                  95e3, 98e3, 99e3, 99.5e3, 100e3};

  for (size_t i = 0; i < results.size(); ++i) {
    auto threshold = 0.1 * expected.at(i);
    EXPECT_NEAR(expected.at(i), results.at(i), threshold);
  }
}

TEST(PercentileBuckets, Percentile) {
  std::array<int64_t, PercentileBucketsLength()> counts{};

  for (int i = 0; i < 100000; ++i) {
    ++counts[PercentileBucketIndexOf(i)];
  }

  std::vector<double> pcts{0.0,  25.0, 50.0, 75.0, 90.0,
                           95.0, 98.0, 99.0, 99.5, 100.0};
  std::vector<double> results;
  Percentiles(counts, pcts, &results);
  ASSERT_EQ(results.size(), pcts.size());

  for (double pct : pcts) {
    auto expected = pct * 1e3;
    auto threshold = 0.1 * expected;
    EXPECT_NEAR(expected, Percentile(counts, pct), threshold);
  }
}