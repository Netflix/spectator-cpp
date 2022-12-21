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

}  // namespace
