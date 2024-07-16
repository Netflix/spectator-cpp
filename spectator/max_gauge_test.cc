#include "stateless_meters.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {

using spectator::Id;
using spectator::MaxGauge;
using spectator::Tags;
using spectator::TestPublisher;

TEST(MaxGauge, Set) {
  TestPublisher publisher;
  auto id = std::make_shared<Id>("gauge", Tags{});
  auto id2 = std::make_shared<Id>("gauge2", Tags{{"key", "val"}});
  MaxGauge g{id, &publisher};
  MaxGauge g2{id2, &publisher};

  g.Set(42);
  g2.Update(2);
  g.Update(1);
  std::vector<std::string> expected = {"m:gauge:42", "m:gauge2,key=val:2",
                                       "m:gauge:1"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

TEST(MaxGauge, InvalidTags) {
  TestPublisher publisher;
  // test with a single tag, because tags order is not guaranteed in a flat_hash_map
  auto id = std::make_shared<Id>("test`!@#$%^&*()-=~_+[]{}\\|;:'\",<.>/?foo",
                                 Tags{{"tag1,:=", "value1,:="}});
  MaxGauge g{id, &publisher};
  EXPECT_EQ("m:test______^____-_~______________.___foo,tag1___=value1___:", g.GetPrefix());
}
}  // namespace
