// Benchmarks different implementations for Tags

#include <benchmark/benchmark.h>
#include <fmt/format.h>
#include <ska/flat_hash_map.hpp>
#include <tsl/hopscotch_map.h>
#include <unordered_map>

template <typename C>
class Tags {
  using K = std::string;
  using V = std::string;
  C entries_;

 public:
  Tags() = default;
  Tags(const Tags& other) = default;

  Tags(std::initializer_list<std::pair<std::string, std::string>> vs) {
    for (auto& pair : vs) {
      add(std::move(pair.first), std::move(pair.second));
    }
  }

  void add(K k, V v) { entries_[std::move(k)] = std::move(v); }

  size_t hash() const {
    size_t h = 0;
    for (const auto& entry : entries_) {
      h += (std::hash<K>()(entry.first) << 1) ^ std::hash<V>()(entry.second);
    }
    return h;
  }

  void add_all(const Tags<C>& source) {
    for (const auto& t : source.entries_) {
      entries_[t.first] = t.second;
    }
  }

  bool operator==(const Tags<C>& that) const {
    return that.entries_ == entries_;
  }

  bool has(const K& key) const { return entries_.find(key) != entries_.end(); }

  K at(const K& key) const {
    auto entry = entries_.find(key);
    if (entry != entries_.end()) {
      return entry->second;
    }
    return {};
  }

  size_t size() const { return entries_.size(); }

  typename C::const_iterator begin() const { return entries_.begin(); }

  typename C::const_iterator end() const { return entries_.end(); }
};

template <typename T>
void add_tags(benchmark::State& state) {
  Tags<T> tags;
  for (auto _ : state) {
    for (auto i = 0; i < state.range(0); ++i) {
      // the majority of our tags usually fit in small strings
      auto key1 = fmt::format("nf.1key{}", i);
      auto key2 = fmt::format("nf.2key{}", i);
      auto key3 = fmt::format("nf.3key{}", i);
      // test how it behaves with a long string
      auto long_key = fmt::format(
          "some.longKey.value.that.does.not.fit.in.the.string.{}", i);

      tags.add(key1, key1);
      tags.add(key2, key2);
      tags.add(key3, key3);
      tags.add(long_key, long_key);
    }
  }
}

template <typename T>
Tags<T> with_stat(const Tags<T>& source, const std::string& stat) {
  Tags<T> copy{source};
  copy.add("statistic", stat);
  return copy;
}

// mimic the `id->WithTag()` functionality which is used very frequently, esp.
// with the Measurements() functionality
template <typename T>
void with_tag(benchmark::State& state) {
  Tags<T> tags{
      {"name", "some.name"}, {"id", "some.id"}, {"dev", "root.device"}};

  for (auto _ : state) {
    // simulate a timer
    benchmark::DoNotOptimize(with_stat(tags, "count"));
    benchmark::DoNotOptimize(with_stat(tags, "totalTime"));
    benchmark::DoNotOptimize(with_stat(tags, "totalOfSquares"));
    benchmark::DoNotOptimize(with_stat(tags, "max"));
  }
}

static void BM_FlatHashMap(benchmark::State& state) {
  srand(1);
  add_tags<ska::flat_hash_map<std::string, std::string>>(state);
}

static void BM_UnorderedMap(benchmark::State& state) {
  srand(1);
  add_tags<std::unordered_map<std::string, std::string>>(state);
}

static void BM_HopscotchMap(benchmark::State& state) {
  srand(1);
  using table_t = tsl::hopscotch_map<std::string, std::string>;
  add_tags<table_t>(state);
}

static void BM_HopscotchMapHash(benchmark::State& state) {
  srand(1);
  // test storing the hash as part of the entry
  using table_t =
      tsl::hopscotch_map<std::string, std::string, std::hash<std::string>,
                         std::equal_to<std::string>,
                         std::allocator<std::pair<std::string, std::string>>, 4,
                         true>;
  add_tags<table_t>(state);
}

static void BM_UnorderedMapWithTag(benchmark::State& state) {
  with_tag<std::unordered_map<std::string, std::string>>(state);
}

static void BM_FlatHashMapWithTag(benchmark::State& state) {
  with_tag<ska::flat_hash_map<std::string, std::string>>(state);
}

static void BM_HopscotchMapWithTag(benchmark::State& state) {
  with_tag<tsl::hopscotch_map<std::string, std::string>>(state);
}

static void BM_HopscotchMapHashWithTag(benchmark::State& state) {
  using table_t =
      tsl::hopscotch_map<std::string, std::string, std::hash<std::string>,
                         std::equal_to<std::string>,
                         std::allocator<std::pair<std::string, std::string>>, 4,
                         true>;
  with_tag<table_t>(state);
}

BENCHMARK(BM_UnorderedMap)->RangeMultiplier(2)->Range(1, 8);
BENCHMARK(BM_FlatHashMap)->RangeMultiplier(2)->Range(1, 8);
BENCHMARK(BM_HopscotchMap)->RangeMultiplier(2)->Range(1, 8);
BENCHMARK(BM_HopscotchMapHash)->RangeMultiplier(2)->Range(1, 8);

BENCHMARK(BM_UnorderedMapWithTag);
BENCHMARK(BM_FlatHashMapWithTag);
BENCHMARK(BM_HopscotchMapWithTag);
BENCHMARK(BM_HopscotchMapHashWithTag);

BENCHMARK_MAIN();
