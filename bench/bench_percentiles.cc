// Benchmarks lazy/eager implementations for percentile timers

#include <benchmark/benchmark.h>
#include <unordered_map>
#include "spectator/percentile_timer.h"

using spectator::IdPtr;
template <class T>
T* get_timer(spectator::Registry* r, int i,
             std::unordered_map<IdPtr, std::unique_ptr<T>>& timers) {
  spectator::Tags tags{
      {"tag-key1", "tag-value1"},
      {"tag-key2", "tag-value2"},
  };
  auto value_idx = rand() % i;
  tags.add("some.key", fmt::format("some-value-{:09}", value_idx));
  IdPtr id = r->CreateId("some.name", std::move(tags));
  auto it = timers.find(id);
  if (it != timers.end()) {
    return it->second.get();
  }

  using secs = std::chrono::seconds;
  using nanos = std::chrono::nanoseconds;

  auto timer_ptr = std::make_unique<T>(r, id, nanos{0}, secs{10});
  T* ret = timer_ptr.get();
  timers.insert(std::make_pair(id, std::move(timer_ptr)));
  return ret;
}

template <typename T>
void BenchPercentiles(benchmark::State& state) {
  std::unordered_map<IdPtr, std::unique_ptr<T>> timers;
  srand(1);
  spectator::Registry r{spectator::GetConfiguration(),
                        spectator::DefaultLogger()};
  for (auto _ : state) {
    auto timer = get_timer<T>(&r, state.range(0), timers);
    auto millis = std::chrono::milliseconds{rand() % state.range(0) * 1000};
    timer->Record(millis);
  }
}

namespace spectator {
namespace detail {
struct eager_policy {
  static void init(Registry* r, const Id* id, counters_t& counters,
                   const std::string* perc_tags) {
    for (auto i = 0u; i < counters.size(); ++i) {
      counters.at(i) = r->GetCounter(id->WithTags(
          {{"statistic", "percentile"}, {"percentile", perc_tags[i]}}));
    }
  }

  static std::shared_ptr<Counter> get_counter(
      Registry* /* r */, const Id* /* id */, counters_t& counters, size_t index,
      const std::string* /* perc_tags */) {
    return counters.at(index);
  }
};
}  // namespace detail
}  // namespace spectator

using spectator::PercentileTimer;
using EagerPercentileTimer =
    spectator::percentile_timer<spectator::detail::eager_policy>;
BENCHMARK_TEMPLATE(BenchPercentiles, PercentileTimer)
    ->RangeMultiplier(16)
    ->Range(8, 1024);
BENCHMARK_TEMPLATE(BenchPercentiles, EagerPercentileTimer)
    ->RangeMultiplier(256)
    ->Range(8, 1024);

BENCHMARK_MAIN();
