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
  Timer t{id, &publisher};
  Timer t2{id2, &publisher};
  t.Record(std::chrono::milliseconds(1));
  t2.Record(absl::Seconds(0.1));
  t2.Record(absl::Microseconds(500));
  std::vector<std::string> expected = {"t:t.name:0.001", "t:t2,key=val:0.1", "t:t2,key=val:0.0005"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

TEST(Timer, InvalidTags) {
  TestPublisher publisher;
  // test with a single tag, because tags order is not guaranteed in a flat_hash_map
  auto id = std::make_shared<Id>("timer`!@#$%^&*()-=~_+[]{}\\|;:'\",<.>/?foo",
                                 Tags{{"tag1,:=", "value1,:="}});
  Timer t{id, &publisher};
  EXPECT_EQ("t:timer______^____-_~______________.___foo,tag1___=value1___:", t.GetPrefix());
}
}  // namespace
