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

  g.Set(42);
  g2.Set(2);
  g.Set(1);
  std::vector<std::string> expected = {"A:gauge:42", "A:gauge2,key=val:2",
                                       "A:gauge:1"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

}  // namespace
