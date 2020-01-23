#include "timer.h"
#include "atomicnumber.h"

namespace spectator {

Timer::Timer(IdPtr id) noexcept
    : id_(std::move(id)), count_(0), total_(0), totalSq_(0.0), max_(0) {}

IdPtr Timer::MeterId() const noexcept { return id_; }

std::vector<Measurement> Timer::Measure() const noexcept {
  std::vector<Measurement> results;
  auto cnt = count_.exchange(0, std::memory_order_relaxed);
  if (cnt == 0) {
    return results;
  }

  auto total = total_.exchange(0, std::memory_order_relaxed);
  auto total_secs = total / 1e9;
  auto t_sq = totalSq_.exchange(0.0, std::memory_order_relaxed);
  auto t_sq_secs = t_sq / 1e18;
  auto mx = max_.exchange(0, std::memory_order_relaxed);
  auto mx_secs = mx / 1e9;
  results.reserve(4);
  results.push_back({id_->WithStat("count"), static_cast<double>(cnt)});
  results.push_back({id_->WithStat("totalTime"), total_secs});
  results.push_back({id_->WithStat("totalOfSquares"), t_sq_secs});
  results.push_back({id_->WithStat("max"), mx_secs});
  return results;
}

void Timer::Record(std::chrono::nanoseconds amount) noexcept {
  int64_t ns = amount.count();
  if (ns >= 0) {
    count_.fetch_add(1, std::memory_order_relaxed);
    total_.fetch_add(ns, std::memory_order_relaxed);
    add_double(&totalSq_, static_cast<double>(ns) * ns);
    update_max(&max_, ns);
  }
}

int64_t Timer::Count() const noexcept {
  return count_.load(std::memory_order_relaxed);
}

int64_t Timer::TotalTime() const noexcept {
  return total_.load(std::memory_order_relaxed);
}

}  // namespace spectator
