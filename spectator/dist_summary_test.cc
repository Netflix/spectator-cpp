#include "stateless_meters.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {
using spectator::DistributionSummary;
using spectator::Id;
using spectator::Tags;
using spectator::TestPublisher;

TEST(DistributionSummary, Record) {
  TestPublisher publisher;
  auto id = std::make_shared<Id>("ds.name", Tags{});
  auto id2 = std::make_shared<Id>("ds2", Tags{{"key", "val"}});
  DistributionSummary d{id, &publisher};
  DistributionSummary d2{id2, &publisher};
  d.Record(10);
  d2.Record(1.2);
  d.Record(0.1);
  std::vector<std::string> expected = {"d:ds.name:10", "d:ds2,key=val:1.2",
                                       "d:ds.name:0.1"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

TEST(DistributionSummary, InvalidTags) {
  TestPublisher publisher;
  // test with a single tag, because tags order is not guaranteed in a flat_hash_map
  auto id = std::make_shared<Id>("test`!@#$%^&*()-=~_+[]{}\\|;:'\",<.>/?foo",
                                 Tags{{"tag1,:=", "value1,:="}});
  DistributionSummary d{id, &publisher};
  EXPECT_EQ("d:test______^____-_~______________.___foo,tag1___=value1___:", d.GetPrefix());
}
}  // namespace
