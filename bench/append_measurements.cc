// Benchmarks publisher#append_measurements

#include <benchmark/benchmark.h>
#include "spectator/publisher.h"
#include <memory>

using spectator::Measurement;

using StrTable = ska::flat_hash_map<std::string, int>;
static StrTable get_string_table() {
  StrTable string_table;
  for (auto i = 0; i < 1000; ++i) {
    string_table[fmt::format("some.prefix.{:04}", i)] = i;
  }
  return string_table;
}

static std::map<std::string, std::string> get_common_tags() {
  static constexpr auto kNumTags = 7;  // assume 7 common tags

  std::map<std::string, std::string> tags;
  for (auto i = 0; i < kNumTags; ++i) {
    auto key = fmt::format("some.prefix.{:04}", 2 * i);
    auto value = fmt::format("some.prefix.{:04}", 2 * i + 1);
    tags[key] = value;
  }
  return tags;
}

static std::vector<int> get_common_tags_ids() {
  static constexpr auto kNumTags = 7;
  std::vector<int> ids;
  ids.reserve(kNumTags * 2);
  for (auto i = 0; i < kNumTags; ++i) {
    ids.push_back(i);
  }
  return ids;
}

template <typename T>
void add_tags(rapidjson::Document* payload, const StrTable& strings,
              const T& tags) {
  using rapidjson::Value;
  auto& alloc = payload->GetAllocator();
  for (const auto& tag : tags) {
    auto k_pair = strings.find(tag.first);
    auto v_pair = strings.find(tag.second);
    assert(k_pair != strings.end());
    assert(v_pair != strings.end());
    payload->PushBack(k_pair->second, alloc);
    payload->PushBack(v_pair->second, alloc);
  }
}

void append_measurement_lookup(
    rapidjson::Document* payload, const StrTable& strings,
    const std::map<std::string, std::string>& common_tags,
    const Measurement& m) {
  auto& alloc = payload->GetAllocator();
  int64_t total_tags = m.id->GetTags().size() + 1 + common_tags.size();
  payload->PushBack(total_tags, alloc);
  add_tags(payload, strings, common_tags);
  add_tags(payload, strings, m.id->GetTags());
  auto name_idx = strings.find("name")->second;
  auto name_value_idx = strings.find(m.id->Name())->second;
  payload->PushBack(name_idx, alloc);
  payload->PushBack(name_value_idx, alloc);
  payload->PushBack(m.value, alloc);
}

void append_measurement_ids(rapidjson::Document* payload,
                            const StrTable& strings,
                            const std::vector<int> common_tags_ids,
                            const Measurement& m) {
  auto& alloc = payload->GetAllocator();
  int64_t total_tags = m.id->GetTags().size() + 1 + common_tags_ids.size() / 2;
  payload->PushBack(total_tags, alloc);
  for (auto i : common_tags_ids) {
    payload->PushBack(i, alloc);
  }
  add_tags(payload, strings, m.id->GetTags());
  auto name_idx = strings.find("name")->second;
  auto name_value_idx = strings.find(m.id->Name())->second;
  payload->PushBack(name_idx, alloc);
  payload->PushBack(name_value_idx, alloc);
  payload->PushBack(m.value, alloc);
}

static spectator::IdPtr get_id() {
  auto tags = spectator::Tags{{{"some.prefix.0100", "some.prefix.0101"},
                               {"some.prefix.0102", "some.prefix.0103"},
                               {"some.prefix.0104", "some.prefix.0105"}}};
  return std::make_shared<spectator::Id>(std::string{"some.prefix.0123"}, tags);
}

static void BM_append_common_tags(benchmark::State& state) {
  // test the behavior where we have to lookup the common tags in
  // append_measurements
  rapidjson::Document payload{rapidjson::kArrayType};
  auto strings = get_string_table();
  auto common_tags = get_common_tags();
  Measurement m{get_id(), 100.0};
  for (auto _ : state) {
    append_measurement_lookup(&payload, strings, common_tags, m);
  }
}
static void BM_append_common_tags_ids(benchmark::State& state) {
  // test the behavior where we have to lookup the common tags in
  // append_measurements
  rapidjson::Document payload{rapidjson::kArrayType};
  auto strings = get_string_table();
  auto ids = get_common_tags_ids();
  Measurement m{get_id(), 100.0};
  for (auto _ : state) {
    append_measurement_ids(&payload, strings, ids, m);
  }
}

BENCHMARK(BM_append_common_tags);
BENCHMARK(BM_append_common_tags_ids);
BENCHMARK_MAIN();
