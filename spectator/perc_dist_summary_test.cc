#include "stateless_meters.h"
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
  PercentileDistributionSummary<TestPublisher> c{id, &publisher, 0, 1000};
  c.Record(50);
  c.Record(5000);
  c.Record(-5000);
  std::vector<std::string> expected = {"D:pds:50", "D:pds:1000",
                                       "D:pds:0"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

}  // namespace
