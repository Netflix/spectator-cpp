#include "dist_summary.h"
#include "atomicnumber.h"

namespace spectator {

DistributionSummary::DistributionSummary(IdPtr id) noexcept
    : id_(std::move(id)), count_(0), total_(0), totalSq_(0.0), max_(0) {}

IdPtr DistributionSummary::MeterId() const noexcept { return id_; }

std::vector<Measurement> DistributionSummary::Measure() const noexcept {
  std::vector<Measurement> results;
  auto cnt = count_.exchange(0, std::memory_order_relaxed);
  if (cnt == 0) {
    return results;
  }

  if (!count_id_) {
    total_id_ = id_->WithStat("totalAmount");
    total_sq_id_ = id_->WithStat("totalOfSquares");
    max_id_ = id_->WithStat("max");
    count_id_ = id_->WithStat("count");
  }

  auto total = total_.exchange(0.0, std::memory_order_relaxed);
  auto t_sq = totalSq_.exchange(0.0, std::memory_order_relaxed);
  auto mx = max_.exchange(0.0, std::memory_order_relaxed);
  results.reserve(4);
  results.push_back({count_id_, static_cast<double>(cnt)});
  results.push_back({total_id_, total});
  results.push_back({total_sq_id_, t_sq});
  results.push_back({max_id_, mx});
  return results;
}

void DistributionSummary::Record(double amount) noexcept {
  Update();

  if (amount >= 0) {
    count_.fetch_add(1, std::memory_order_relaxed);
    add_double(&total_, amount);
    add_double(&totalSq_, amount * amount);
    update_max(&max_, amount);
  }
}

int64_t DistributionSummary::Count() const noexcept {
  return count_.load(std::memory_order_relaxed);
}

double DistributionSummary::TotalAmount() const noexcept {
  return total_.load(std::memory_order_relaxed);
}

}  // namespace spectator
