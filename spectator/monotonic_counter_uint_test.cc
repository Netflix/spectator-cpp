#include "stateless_meters.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {

using spectator::Id;
using spectator::MonotonicCounterUint;
using spectator::Tags;
using spectator::TestPublisher;

TEST(MonotonicCounterUint, Set) {
  TestPublisher publisher;
  auto id = std::make_shared<Id>("ctr", Tags{});
  auto id2 = std::make_shared<Id>("ctr2", Tags{{"key", "val"}});
  MonotonicCounterUint<TestPublisher> c{id, &publisher};
  MonotonicCounterUint<TestPublisher> c2{id2, &publisher};

  c.Set(42);
  c2.Set(2);
  c.Set(43);
  std::vector<std::string> expected = {"U:ctr:42", "U:ctr2,key=val:2", "U:ctr:43"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}
}  // namespace