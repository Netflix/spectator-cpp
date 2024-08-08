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
  PercentileDistributionSummary d{id, &publisher, 0, 1000};
  d.Record(50);
  d.Record(5000);
  d.Record(-5000);
  std::vector<std::string> expected = {"D:pds:50", "D:pds:1000",
                                       "D:pds:0"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

TEST(PercDistSum, InvalidTags) {
  TestPublisher publisher;
  // test with a single tag, because tags order is not guaranteed in a flat_hash_map
  auto id = std::make_shared<Id>("test`!@#$%^&*()-=~_+[]{}\\|;:'\",<.>/?foo",
                                 Tags{{"tag1,:=", "value1,:="}});
  PercentileDistributionSummary d{id, &publisher, 0, 1000};
  EXPECT_EQ("D:test______^____-_~______________.___foo,tag1___=value1___:", d.GetPrefix());
}
}  // namespace
