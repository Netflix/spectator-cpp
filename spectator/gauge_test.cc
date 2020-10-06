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
  std::vector<std::string> expected = {"1:g:gauge:42", "1:g:gauge2:#key=val:2",
                                       "1:g:gauge:1"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

}  // namespace
