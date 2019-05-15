#include "percentile_distribution_summary.h"
#include "percentile_bucket_tags.inc"

using nanos = std::chrono::nanoseconds;

namespace spectator {

PercentileDistributionSummary::PercentileDistributionSummary(
    spectator::Registry* registry, spectator::IdPtr id) noexcept
    : registry_{registry},
      id_{std::move(id)},
      dist_summary_{registry->GetDistributionSummary(id_)} {}

void PercentileDistributionSummary::Record(int64_t amount) noexcept {
  dist_summary_->Record(amount);
  auto index = PercentileBucketIndexOf(amount);
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
      counts.at(i) = c->Count();
    }
  }
  return spectator::Percentile(counts, p);
}

}  // namespace spectator
