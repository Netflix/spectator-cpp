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
  Counter<TestPublisher> c{id, &publisher};
  Counter<TestPublisher> c2{id2, &publisher};
  c.Increment();
  c2.Add(1.2);
  c.Add(0.1);
  std::vector<std::string> expected = {"c:ctr.name:1", "c:c2,key=val:1.2",
                                       "c:ctr.name:0.1"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

TEST(Counter, Id) {
  TestPublisher publisher;
  Counter<TestPublisher> c{std::make_shared<Id>("foo", Tags{{"key", "val"}}),
                           &publisher};
  auto id = std::make_shared<Id>("foo", Tags{{"key", "val"}});
  EXPECT_EQ(*(c.MeterId()), *id);
}

}  // namespace
