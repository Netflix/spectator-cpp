#include "stateless_meters.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {

using spectator::Gauge;
using spectator::Id;
using spectator::Tags;
using spectator::TestPublisher;

TEST(Gauge, Set) {
  TestPublisher publisher;
  auto id = std::make_shared<Id>("gauge", Tags{});
  auto id2 = std::make_shared<Id>("gauge2", Tags{{"key", "val"}});
  Gauge g{id, &publisher};
  Gauge g2{id2, &publisher};

  g.Set(42);
  g2.Set(2);
  g.Set(1);
  std::vector<std::string> expected = {"g:gauge:42", "g:gauge2,key=val:2",
                                       "g:gauge:1"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

TEST(Gauge, SetWithTTL) {
  TestPublisher publisher;
  auto id = std::make_shared<Id>("gauge", Tags{});
  auto id2 = std::make_shared<Id>("gauge2", Tags{{"key", "val"}});
  Gauge g{id, &publisher, 1};
  Gauge g2{id2, &publisher, 2};

  g.Set(42);
  g2.Set(2);
  g.Set(1);
  std::vector<std::string> expected = {"g,1:gauge:42", "g,2:gauge2,key=val:2",
                                       "g,1:gauge:1"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}


TEST(Gauge, InvalidTags) {
  TestPublisher publisher;
  // test with a single tag, because tags order is not guaranteed in a flat_hash_map
  auto id = std::make_shared<Id>("test`!@#$%^&*()-=~_+[]{}\\|;:'\",<.>/?foo",
                                 Tags{{"tag1,:=", "value1,:="}});
  Gauge g{id, &publisher};
  EXPECT_EQ("g:test______^____-_~______________.___foo,tag1___=value1___:", g.GetPrefix());
}
}  // namespace
