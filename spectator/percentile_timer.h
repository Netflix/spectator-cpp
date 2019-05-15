
#pragma once

#include <chrono>
#include "id.h"
#include "registry.h"
#include "percentile_buckets.h"
namespace spectator {

class PercentileTimer {
 public:
  PercentileTimer(Registry* registry, IdPtr id, std::chrono::nanoseconds min,
                  std::chrono::nanoseconds max) noexcept;

  void Record(std::chrono::nanoseconds amount) noexcept;
  int64_t Count() const noexcept { return timer_->Count(); }
  int64_t TotalTime() const noexcept { return timer_->TotalTime(); }
  double Percentile(double p) const noexcept;

 private:
  Registry* registry_;
  IdPtr id_;
  std::chrono::nanoseconds min_;
  std::chrono::nanoseconds max_;
  std::shared_ptr<Timer> timer_;
  mutable std::array<std::shared_ptr<Counter>, PercentileBucketsLength()>
      counters_{};
};

}  // namespace spectator
