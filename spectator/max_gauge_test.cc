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

}  // namespace
