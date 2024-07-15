#include "stateless_meters.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {

using spectator::Id;
using spectator::AgeGauge;
using spectator::Tags;
using spectator::TestPublisher;

TEST(AgeGauge, Set) {
  TestPublisher publisher;
  auto id = std::make_shared<Id>("gauge", Tags{});
  auto id2 = std::make_shared<Id>("gauge2", Tags{{"key", "val"}});
  AgeGauge g{id, &publisher};
  AgeGauge g2{id2, &publisher};

  g.Set(1671641328);
  g2.Set(1671641028.3);
  g.Set(0);
  std::vector<std::string> expected = {"A:gauge:1671641328",
                                       "A:gauge2,key=val:1671641028.3",
                                       "A:gauge:0"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

TEST(AgeGauge, InvalidTags) {
  TestPublisher publisher;
  // test with a single tag, because tags order is not guaranteed in a flat_hash_map
  auto id = std::make_shared<Id>("test`!@#$%^&*()-=~_+[]{}\\|;:'\",<.>/?foo",
                                 Tags{{"tag1,:=", "value1,:="}});
  AgeGauge g{id, &publisher};
  EXPECT_EQ("A:test______^____-_~______________.___foo,tag1___=value1___:", g.GetPrefix());
}
}  // namespace
