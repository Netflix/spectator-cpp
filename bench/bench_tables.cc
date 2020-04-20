// Benchmarks different implementations for the registry

#include <benchmark/benchmark.h>
#include <ska/flat_hash_map.hpp>
#include <unordered_map>
#include "spectator/registry.h"
#include <tsl/hopscotch_map.h>

using spectator::Counter;
using spectator::IdPtr;
using spectator::Meter;
using spectator::Tags;

static IdPtr get_id(int i) {
  Tags tags{
      {"one-relatively-long-tag-key",
       "some-relatively-long-tag-value-aaasdfadf"},
      {"short", "key"},
  };
  auto value_idx = rand() % i;
  tags.add("some.key", fmt::format("some-value-{:09}", value_idx));
  return std::make_shared<spectator::Id>("some.counter.name", tags);
}

static std::shared_ptr<Counter> get_counter(int i) {
  return std::make_shared<Counter>(get_id(i));
}

template <typename T>
void BenchTable(benchmark::State& state) {
  T table;
  srand(1);
  for (auto _ : state) {
    auto counter = get_counter(state.range(0));
    benchmark::DoNotOptimize(table.insert({counter->MeterId(), counter}));
  }
}

using meter_ptr = std::shared_ptr<Meter>;
using flat=ska::flat_hash_map<IdPtr, meter_ptr>;
using unordered=std::unordered_map<IdPtr, meter_ptr>;
using hop=tsl::hopscotch_map<IdPtr, meter_ptr>;
using hophash=tsl::hopscotch_map<IdPtr, meter_ptr, std::hash<IdPtr>,
    std::equal_to<IdPtr>, std::allocator<std::pair<IdPtr, meter_ptr>>, 30, true>;
BENCHMARK_TEMPLATE(BenchTable, unordered)->RangeMultiplier(256)->Range(8, 8 << 20);
BENCHMARK_TEMPLATE(BenchTable, flat)->RangeMultiplier(256)->Range(8, 8 << 20);
BENCHMARK_TEMPLATE(BenchTable, hop)->RangeMultiplier(256)->Range(8, 8 << 20);
BENCHMARK_TEMPLATE(BenchTable, hophash)->RangeMultiplier(256)->Range(8, 8 << 20);

BENCHMARK_MAIN();
