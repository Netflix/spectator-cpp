#include "../spectator/memory.h"
#include "../spectator/timer.h"
#include <gtest/gtest.h>
#include <unordered_map>

namespace {
using spectator::Timer;

std::unique_ptr<Timer> getTimer() {
  auto id = std::make_shared<spectator::Id>("t", spectator::Tags{});
  return std::make_unique<Timer>(id);
}

TEST(Timer, Record) {
  auto t = getTimer();
  // quick sanity check
  EXPECT_EQ(t->Count(), 0);
  EXPECT_EQ(t->TotalTime(), 0);

  t->Record(std::chrono::microseconds(1));
  EXPECT_EQ(t->Count(), 1);
  EXPECT_EQ(t->TotalTime(), 1000);

  t->Record(std::chrono::seconds(0));
  EXPECT_EQ(t->Count(), 2);
  EXPECT_EQ(t->TotalTime(), 1000);

  t->Record(std::chrono::nanoseconds(101));
  EXPECT_EQ(t->Count(), 3);
  EXPECT_EQ(t->TotalTime(), 1101);

  t->Record(std::chrono::seconds(-1));
  EXPECT_EQ(t->Count(), 3);
  EXPECT_EQ(t->TotalTime(), 1101);
}

void expect_timer(const Timer& t, int64_t count, int64_t total, double total_sq,
                  int64_t max) {
  auto ms = t.Measure();
  ASSERT_EQ(ms.size(), 4) << "Expected 4 measurements from a Timer, got "
                          << ms.size();

  std::unordered_map<spectator::IdPtr, double> expected;
  auto id = t.MeterId();
  expected[id->WithStat("count")] = count;
  expected[id->WithStat("totalTime")] = total / 1e9;
  expected[id->WithStat("totalOfSquares")] = total_sq / 1e18;
  expected[id->WithStat("max")] = max / 1e9;

  for (const auto& m : ms) {
    auto it = expected.find(m.id);
    ASSERT_TRUE(it != expected.end()) << "Unexpected result: " << m;
    EXPECT_DOUBLE_EQ(it->second, m.value)
        << "Wrong value for measurement " << m << " expected " << it->second;
    expected.erase(it);  // done processing this entry
  }
  ASSERT_EQ(expected.size(), 0);

  EXPECT_TRUE(t.Measure().empty()) << "resets after measuring";
}

TEST(Timer, Measure) {
  auto t = getTimer();
  EXPECT_TRUE(t->Measure().empty());

  t->Record(std::chrono::nanoseconds(100));
  t->Record(std::chrono::nanoseconds(200));
  expect_timer(*t, 2, 300, 100 * 100 + 200 * 200, 200);
}

TEST(Timer, Updated) {
  auto timer = getTimer();
  timer->Record(std::chrono::nanoseconds(1));

  auto t1 = timer->Updated();
  auto t2 = timer->Updated();
  EXPECT_EQ(t1, t2);

  usleep(1);
  timer->Record(std::chrono::nanoseconds(1));
  EXPECT_TRUE(timer->Updated() > t1);
}
}  // namespace
