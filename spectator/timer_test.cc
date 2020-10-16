#include "stateless_meters.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {
using spectator::Id;
using spectator::Tags;
using spectator::TestPublisher;
using spectator::Timer;

TEST(Timer, Record) {
  TestPublisher publisher;
  auto id = std::make_shared<Id>("t.name", Tags{});
  auto id2 = std::make_shared<Id>("t2", Tags{{"key", "val"}});
  Timer<TestPublisher> t{id, &publisher};
  Timer<TestPublisher> t2{id2, &publisher};
  t.Record(std::chrono::milliseconds(1));
  t2.Record(absl::Seconds(0.1));
  t2.Record(absl::Microseconds(500));
  std::vector<std::string> expected = {
      "t:t.name:0.001", "t:t2,key=val:0.1", "t:t2,key=val:0.0005"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

}  // namespace
