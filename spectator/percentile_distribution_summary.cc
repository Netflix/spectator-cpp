#include "percentile_distribution_summary.h"
#include "percentile_bucket_tags.inc"
#include "util.h"

using nanos = std::chrono::nanoseconds;

namespace spectator {

PercentileDistributionSummary::PercentileDistributionSummary(
    spectator::Registry* registry, spectator::IdPtr id, int64_t min,
    int64_t max) noexcept
    : registry_{registry},
      id_{std::move(id)},
      min_{min},
      max_{max},
      dist_summary_{registry->GetDistributionSummary(id_)} {}

void PercentileDistributionSummary::Record(int64_t amount) noexcept {
  if (amount < 0) {
    return;
  }

  dist_summary_->Record(amount);
  auto restricted = restrict(amount, min_, max_);
  auto index = PercentileBucketIndexOf(restricted);
  auto& c = counters_.at(index);
  if (!c) {
    auto counterId =
        id_->WithStat("percentile")->WithTag("percentile", kDistTags.at(index));
    counters_.at(index) = registry_->GetCounter(std::move(counterId));
    c = counters_.at(index);
  }
  c->Increment();
}

double PercentileDistributionSummary::Percentile(double p) const noexcept {
  std::array<int64_t, PercentileBucketsLength()> counts{};
  for (size_t i = 0; i < PercentileBucketsLength(); ++i) {
    auto& c = counters_.at(i);
    if (c) {
      counts.at(i) = static_cast<int64_t>(c->Count());
    }
  }
  return spectator::Percentile(counts, p);
}

}  // namespace spectator
