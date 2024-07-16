#include "stateless_meters.h"
#include "test_publisher.h"
#include <gtest/gtest.h>

namespace {
using spectator::Id;
using spectator::PercentileTimer;
using spectator::Tags;
using spectator::TestPublisher;

TEST(PercentileTimer, Record) {
  TestPublisher publisher;
  auto id = std::make_shared<Id>("pt", Tags{});
  PercentileTimer c{id, &publisher, absl::ZeroDuration(), absl::Seconds(5)};
  c.Record(absl::Milliseconds(42));
  c.Record(std::chrono::microseconds(500));
  c.Record(absl::Seconds(10));
  std::vector<std::string> expected = {"T:pt:0.042", "T:pt:0.0005", "T:pt:5"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

TEST(PercentileTimer, InvalidTags) {
  TestPublisher publisher;
  // test with a single tag, because tags order is not guaranteed in a flat_hash_map
  auto id = std::make_shared<Id>("test`!@#$%^&*()-=~_+[]{}\\|;:'\",<.>/?foo",
                                 Tags{{"tag1,:=", "value1,:="}});
  PercentileTimer t{id, &publisher, absl::ZeroDuration(), absl::Seconds(5)};
  EXPECT_EQ("T:test______^____-_~______________.___foo,tag1___=value1___:", t.GetPrefix());
}
}  // namespace
