
#pragma once

#include <chrono>
#include "id.h"
#include "percentile_buckets.h"
#include "registry.h"
#include "util.h"
#include "detail/perc_policy.h"

namespace spectator {

template <typename policy>
class percentile_timer {
 public:
  percentile_timer(Registry* registry, IdPtr id, std::chrono::nanoseconds min,
                   std::chrono::nanoseconds max) noexcept
      : registry_{registry},
        id_{std::move(id)},
        min_{min},
        max_{max},
        timer_{registry->GetTimer(id_)} {
    policy::init(registry_, id_.get(), counters_, detail::kTimerTags.begin());
  }

  void Record(std::chrono::nanoseconds amount) noexcept {
    timer_->Record(amount);
    auto restricted = restrict(amount, min_, max_);
    auto index = PercentileBucketIndexOf(restricted.count());
    auto c = policy::get_counter(registry_, id_.get(), counters_, index,
                                 detail::kTimerTags.begin());
    c->Increment();
  }

  IdPtr MeterId() const noexcept { return id_; }
  int64_t Count() const noexcept { return timer_->Count(); }
  int64_t TotalTime() const noexcept { return timer_->TotalTime(); }
  double Percentile(double p) const noexcept {
    std::array<int64_t, PercentileBucketsLength()> counts{};
    for (size_t i = 0; i < PercentileBucketsLength(); ++i) {
      auto& c = counters_.at(i);
      if (c) {
        counts.at(i) = c->Count();
      }
    }
    auto v = spectator::Percentile(counts, p);
    return v / 1e9;
  };

 private:
  Registry* registry_;
  IdPtr id_;
  std::chrono::nanoseconds min_;
  std::chrono::nanoseconds max_;
  std::shared_ptr<Timer> timer_;
  mutable detail::counters_t counters_{};
};

using PercentileTimer = percentile_timer<detail::lazy_policy>;
using EagerPercentileTimer = percentile_timer<detail::eager_policy>;

}  // namespace spectator
