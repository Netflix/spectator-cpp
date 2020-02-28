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
void TestTable(benchmark::State& state) {
  T table;
  for (auto _ : state) {
    auto counter = get_counter(state.range(0));
    benchmark::DoNotOptimize(table.insert({counter->MeterId(), counter}));
  }
}

using meter_ptr = std::shared_ptr<Meter>;
static void BM_FlatHashMap(benchmark::State& state) {
  srand(1);
  TestTable<ska::flat_hash_map<IdPtr, meter_ptr>>(state);
}

static void BM_UnorderedMap(benchmark::State& state) {
  srand(1);
  TestTable<std::unordered_map<IdPtr, meter_ptr>>(state);
}

static void BM_HopscotchMap(benchmark::State& state) {
  srand(1);
  using table_t = tsl::hopscotch_map<IdPtr, meter_ptr>;
  TestTable<table_t>(state);
}

static void BM_HopscotchMapHash(benchmark::State& state) {
  srand(1);
  // test storing the hash as part of the entry
  using table_t =
      tsl::hopscotch_map<IdPtr, meter_ptr, std::hash<IdPtr>,
                         std::equal_to<IdPtr>,
                         std::allocator<std::pair<IdPtr, meter_ptr>>, 30, true>;
  TestTable<table_t>(state);
}

BENCHMARK(BM_FlatHashMap)->RangeMultiplier(256)->Range(8, 8 << 20);
BENCHMARK(BM_UnorderedMap)->RangeMultiplier(256)->Range(8, 8 << 20);
BENCHMARK(BM_HopscotchMap)->RangeMultiplier(256)->Range(8, 8 << 20);
BENCHMARK(BM_HopscotchMapHash)->RangeMultiplier(256)->Range(8, 8 << 20);

BENCHMARK_MAIN();
