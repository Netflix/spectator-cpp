#pragma once

namespace spectator {
namespace detail {
#include "percentile_bucket_tags.inc"

using counters_t =
    std::array<std::shared_ptr<Counter>, PercentileBucketsLength()>;

struct lazy_policy {
  static void init(Registry* /* r */, const Id* /* id */,
                   counters_t& /* counters */,
                   const std::string* /* perc_tags */) {}
  static std::shared_ptr<Counter> get_counter(Registry* r, const Id* id,
                                              counters_t& counters,
                                              size_t index,
                                              const std::string* perc_tags) {
    auto& c = counters.at(index);
    if (!c) {
      auto counterId =
          id->WithStat("percentile")->WithTag("percentile", perc_tags[index]);
      counters.at(index) = r->GetCounter(std::move(counterId));
      c = counters.at(index);
    }
    return c;
  }
};

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