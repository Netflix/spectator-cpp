#pragma once

#include <chrono>
#include "id.h"
#include "registry.h"
#include "percentile_buckets.h"
#include "util.h"
#include "detail/perc_policy.h"

namespace spectator {

template <typename policy>
class percentile_distribution_summary {
 public:
  percentile_distribution_summary(Registry* registry, IdPtr id, int64_t min,
                                int64_t max) noexcept: registry_{registry},
  id_{std::move(id)},
  min_{min},
  max_{max},
  dist_summary_{registry->GetDistributionSummary(id_)} {
    policy::init(registry_, id_.get(), counters_, detail::kDistTags.begin());
  }

  void Record(int64_t amount) noexcept {
    if (amount < 0) {
      return;
    }

    dist_summary_->Record(amount);
    auto restricted = restrict(amount, min_, max_);
    auto index = PercentileBucketIndexOf(restricted);
    auto c = policy::get_counter(registry_, id_.get(), counters_, index, detail::kDistTags.begin());
    c->Increment();
  }

  IdPtr MeterId() const noexcept { return id_; }
  int64_t Count() const noexcept { return dist_summary_->Count(); }
  double TotalAmount() const noexcept { return dist_summary_->TotalAmount(); }
  double Percentile(double p) const noexcept {
    std::array<int64_t, PercentileBucketsLength()> counts{};
    for (size_t i = 0; i < PercentileBucketsLength(); ++i) {
      auto& c = counters_.at(i);
      if (c) {
        counts.at(i) = static_cast<int64_t>(c->Count());
      }
    }
    return spectator::Percentile(counts, p);
  }

 private:
  Registry* registry_;
  IdPtr id_;
  int64_t min_;
  int64_t max_;
  std::shared_ptr<DistributionSummary> dist_summary_;
  mutable detail::counters_t counters_{};
};

using PercentileDistributionSummary = percentile_distribution_summary<detail::lazy_policy>;
using EagerPercentileDistributionSummary = percentile_distribution_summary<detail::eager_policy>;

}  // namespace spectator
