#include "percentile_timer.h"
#include "percentile_bucket_tags.inc"

using nanos = std::chrono::nanoseconds;

namespace spectator {

PercentileTimer::PercentileTimer(spectator::Registry* registry,
                                 spectator::IdPtr id,
                                 std::chrono::nanoseconds min,
                                 std::chrono::nanoseconds max) noexcept
    : registry_{registry},
      id_{std::move(id)},
      min_{min},
      max_{max},
      timer_{registry->GetTimer(id_)} {}

static nanos restrict(nanos amount, nanos min, nanos max) {
  auto r = amount;
  if (r > max) {
    r = max;
  } else if (r < min) {
    r = min;
  }
  return r;
}

void PercentileTimer::Record(std::chrono::nanoseconds amount) noexcept {
  timer_->Record(amount);
  auto restricted = restrict(amount, min_, max_);
  auto index = PercentileBucketIndexOf(restricted.count());
  auto& c = counters_.at(index);
  if (!c) {
    auto counterId = id_->WithStat("percentile")
                         ->WithTag("percentile", kTimerTags.at(index));
    counters_.at(index) = registry_->GetCounter(std::move(counterId));
    c = counters_.at(index);
  }
  c->Increment();
}

double PercentileTimer::Percentile(double p) const noexcept {
  std::array<int64_t, PercentileBucketsLength()> counts{};
  for (size_t i = 0; i < PercentileBucketsLength(); ++i) {
    auto& c = counters_.at(i);
    if (c) {
      counts.at(i) = c->Count();
    }
  }
  auto v = spectator::Percentile(counts, p);
  return v / 1e9;
}

}  // namespace spectator
