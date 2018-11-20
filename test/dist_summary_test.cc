#include "../spectator/dist_summary.h"
#include "../spectator/memory.h"
#include <gtest/gtest.h>
#include <unordered_map>

namespace {
using spectator::DistributionSummary;

std::unique_ptr<DistributionSummary> getDS() {
  auto id = std::make_shared<spectator::Id>("ds", spectator::Tags{});
  return std::make_unique<DistributionSummary>(id);
}

TEST(DistributionSummary, Record) {
  auto ds = getDS();
  // quick sanity check
  EXPECT_EQ(ds->Count(), 0);
  EXPECT_EQ(ds->TotalAmount(), 0);

  ds->Record(100);
  EXPECT_EQ(ds->Count(), 1);
  EXPECT_EQ(ds->TotalAmount(), 100);

  ds->Record(0);
  EXPECT_EQ(ds->Count(), 2);
  EXPECT_EQ(ds->TotalAmount(), 100);

  ds->Record(101);
  EXPECT_EQ(ds->Count(), 3);
  EXPECT_EQ(ds->TotalAmount(), 201);

  ds->Record(-1);
  EXPECT_EQ(ds->Count(), 3);
  EXPECT_EQ(ds->TotalAmount(), 201);
}

void expect_dist_summary(const DistributionSummary& ds, int64_t count,
                         int64_t total, double total_sq, int64_t max) {
  auto ms = ds.Measure();
  ASSERT_EQ(ms.size(), 4)
      << "Expected 4 measurements from a DistributionSummary, got "
      << ms.size();

  std::unordered_map<spectator::IdPtr, double> expected;
  auto id = ds.MeterId();
  expected[id->WithStat("count")] = count;
  expected[id->WithStat("totalAmount")] = total;
  expected[id->WithStat("totalOfSquares")] = total_sq;
  expected[id->WithStat("max")] = max;

  for (const auto& m : ms) {
    auto it = expected.find(m.id);
    ASSERT_TRUE(it != expected.end()) << "Unexpected result: " << m;
    EXPECT_DOUBLE_EQ(it->second, m.value)
        << "Wrong value for measurement " << m << " expected " << it->second;
    expected.erase(it);  // done processing this entry
  }
  ASSERT_EQ(expected.size(), 0);

  EXPECT_TRUE(ds.Measure().empty()) << "Resets after measuring";
}

TEST(DistributionSummary, Measure) {
  auto ds = getDS();
  EXPECT_TRUE(ds->Measure().empty());

  ds->Record(100);
  ds->Record(200);
  expect_dist_summary(*ds, 2, 300, 100 * 100 + 200 * 200, 200);
}
}  // namespace
