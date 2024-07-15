#include "stateless_meters.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {

using spectator::Counter;
using spectator::Id;
using spectator::Tags;
using spectator::TestPublisher;

TEST(Counter, Activity) {
  TestPublisher publisher;
  auto id = std::make_shared<Id>("ctr.name", Tags{});
  auto id2 = std::make_shared<Id>("c2", Tags{{"key", "val"}});
  Counter c{id, &publisher};
  Counter c2{id2, &publisher};
  c.Increment();
  c2.Add(1.2);
  c.Add(0.1);
  std::vector<std::string> expected = {"c:ctr.name:1", "c:c2,key=val:1.2",
                                       "c:ctr.name:0.1"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

TEST(Counter, Id) {
  TestPublisher publisher;
  Counter c{std::make_shared<Id>("foo", Tags{{"key", "val"}}),
                           &publisher};
  auto id = std::make_shared<Id>("foo", Tags{{"key", "val"}});
  EXPECT_EQ(*(c.MeterId()), *id);
}

TEST(Counter, InvalidTags) {
  TestPublisher publisher;
  // test with a single tag, because tags order is not guaranteed in a flat_hash_map
  auto id = std::make_shared<Id>("test`!@#$%^&*()-=~_+[]{}\\|;:'\",<.>/?foo",
                                 Tags{{"tag1,:=", "value1,:="}});
  Counter c{id, &publisher};
  EXPECT_EQ("c:test______^____-_~______________.___foo,tag1___=value1___:", c.GetPrefix());
}
}  // namespace
