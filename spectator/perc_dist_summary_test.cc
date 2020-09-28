#include "percentile_distribution_summary.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {
using spectator::Id;
using spectator::PercentileDistributionSummary;
using spectator::Tags;
using spectator::TestPublisher;

TEST(PercDistSum, Record) {
  TestPublisher publisher;
  auto id = std::make_shared<Id>("pds", Tags{});
  PercentileDistributionSummary c{id, &publisher, 0, 1000};
  c.Record(50);
  c.Record(5000);
  c.Record(-5000);
  std::vector<std::string> expected = {"1:D:pds:50", "1:D:pds:1000",
                                       "1:D:pds:0"};
  EXPECT_EQ(publisher.Measurements(), expected);
}

}  // namespace
