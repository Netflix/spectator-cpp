#include <fmt/format.h>
#include "test_utils.h"

std::string id_to_string(const spectator::Id& id) {
  std::string res = id.Name();
  const auto& tags = id.GetTags();
  std::map<std::string, std::string> sorted_tags{tags.begin(), tags.end()};

  for (const auto& pair : sorted_tags) {
    res += fmt::format("|{}={}", pair.first, pair.second);
  }
  return res;
}

std::map<std::string, double> measurements_to_map(
    const std::vector<spectator::Measurement>& measurements) {
  auto res = std::map<std::string, double>();
  for (const auto& m : measurements) {
    res[id_to_string(*m.id)] = m.value;
  }
  return res;
}
