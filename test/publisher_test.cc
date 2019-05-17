#include "../spectator/publisher.h"
#include "../spectator/registry.h"
#include <gtest/gtest.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

using spectator::Config;
using spectator::DefaultLogger;
using spectator::Measurement;
using spectator::Publisher;
using spectator::Registry;

namespace {

class TestRegistry : public Registry {
 public:
  TestRegistry(std::unique_ptr<Config> config)
      : Registry(std::move(config), DefaultLogger()) {}
};

class TestPublisher : public Publisher<TestRegistry> {
 public:
  explicit TestPublisher(TestRegistry* test_registry)
      : Publisher(test_registry) {}

  rapidjson::Document ms_to_json(
      std::vector<Measurement>::const_iterator first,
      std::vector<Measurement>::const_iterator last) {
    return measurements_to_json(first, last);
  }
};

std::unique_ptr<Config> get_test_config() {
  auto config = spectator::GetConfiguration();
  config->common_tags["app"] = "atlas";
  config->common_tags["node"] = "i-1234";
  return config;
}

struct payload_entry {
  std::map<std::string, std::string> tags;
  int op;
  double value;

  bool operator==(const payload_entry& other) const {
    return tags == other.tags && op == other.op &&
           std::abs(other.value - value) < 1e-9;
  }
};

std::pair<int, payload_entry> get_entry(const std::vector<std::string>& strings,
                                        const rapidjson::Document& payload,
                                        int base) {
  auto num_tags = payload[base].GetInt();
  std::map<std::string, std::string> tags;
  for (auto i = base + 1; i < base + num_tags * 2; i += 2) {
    auto key_idx = payload[i].GetInt();
    auto val_idx = payload[i + 1].GetInt();
    tags[strings.at(key_idx)] = strings.at(val_idx);
  }
  auto op = payload[base + num_tags * 2 + 1].GetInt();
  auto val = payload[base + num_tags * 2 + 2].GetDouble();
  auto num_consumed = num_tags * 2 + 3;

  return std::make_pair(num_consumed, payload_entry{tags, op, val});
}

std::vector<payload_entry> payload_to_entries(
    const rapidjson::Document& payload) {
  auto num_strings = payload[0].GetInt();
  std::vector<std::string> strings;
  strings.resize(num_strings);
  for (auto i = 1; i <= num_strings; ++i) {
    strings[i - 1] = payload[i].GetString();
  }

  std::vector<payload_entry> entries;
  auto cur_idx = num_strings + 1;
  int num_consumed;
  payload_entry entry;
  while (cur_idx < payload.Size()) {
    std::tie(num_consumed, entry) = get_entry(strings, payload, cur_idx);
    entries.emplace_back(std::move(entry));
    if (num_consumed == 0) {
      std::cerr << "Could not decode payload. Last index: " << cur_idx << "\n";
      abort();
    }
    cur_idx += num_consumed;
  }
  return entries;
}

TEST(Publisher, measurements_to_json) {
  TestRegistry test_registry{get_test_config()};
  TestPublisher publisher{&test_registry};

  auto ctr = test_registry.GetCounter("counter");
  ctr->Increment();
  auto gauge = test_registry.GetGauge("gauge");
  gauge->Set(42.0);

  auto measurements = test_registry.Measurements();
  auto payload = publisher.ms_to_json(measurements.begin(), measurements.end());

  auto entries = payload_to_entries(payload);

  std::vector<payload_entry> expected;
  std::map<std::string, std::string> counter_tags{
      test_registry.GetConfig().common_tags};
  counter_tags["statistic"] = "count";
  counter_tags["name"] = "counter";

  std::map<std::string, std::string> gauge_tags{
      test_registry.GetConfig().common_tags};
  gauge_tags["statistic"] = "gauge";
  gauge_tags["name"] = "gauge";

  expected.emplace_back(payload_entry{counter_tags, 0, 1.0});
  expected.emplace_back(payload_entry{gauge_tags, 10, 42.0});
  EXPECT_EQ(entries, expected);
}

}  // namespace
