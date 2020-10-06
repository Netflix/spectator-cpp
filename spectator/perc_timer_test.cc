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
  PercentileTimer<TestPublisher> c{id, &publisher, absl::ZeroDuration(),
                                   absl::Seconds(5)};
  c.Record(absl::Milliseconds(42));
  c.Record(std::chrono::microseconds(500));
  c.Record(absl::Seconds(10));
  std::vector<std::string> expected = {"1:T:pt:0.042", "1:T:pt:0.0005",
                                       "1:T:pt:5"};
  EXPECT_EQ(publisher.SentMessages(), expected);
}

}  // namespace
