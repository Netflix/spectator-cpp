#pragma once

#include <chrono>
#include "id.h"
#include "registry.h"
#include "percentile_buckets.h"
namespace spectator {

class PercentileDistributionSummary {
 public:
  PercentileDistributionSummary(Registry* registry, IdPtr id, int64_t min,
                                int64_t max) noexcept;

  void Record(int64_t amount) noexcept;
  int64_t Count() const noexcept { return dist_summary_->Count(); }
  double TotalAmount() const noexcept { return dist_summary_->TotalAmount(); }
  double Percentile(double p) const noexcept;

 private:
  Registry* registry_;
  IdPtr id_;
  int64_t min_;
  int64_t max_;
  std::shared_ptr<DistributionSummary> dist_summary_;
  mutable std::array<std::shared_ptr<Counter>, PercentileBucketsLength()>
      counters_{};
};

}  // namespace spectator
