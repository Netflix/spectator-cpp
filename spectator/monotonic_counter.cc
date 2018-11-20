#include "monotonic_counter.h"

namespace spectator {

static constexpr auto kNaN = std::numeric_limits<double>::quiet_NaN();

MonotonicCounter::MonotonicCounter(IdPtr id) noexcept
    : id_{id}, value_(kNaN), prev_value_(kNaN) {}

IdPtr MonotonicCounter::MeterId() const noexcept { return id_; }

void MonotonicCounter::Set(double amount) noexcept {
  value_.store(amount, std::memory_order_relaxed);
}

double MonotonicCounter::Delta() const noexcept {
  return value_.load(std::memory_order_relaxed) -
         prev_value_.load(std::memory_order_relaxed);
}

std::vector<Measurement> MonotonicCounter::Measure() const noexcept {
  auto delta = Delta();
  prev_value_.store(value_.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
  if (delta > 0) {
    return std::vector<Measurement>({{id_->WithStat("count"), delta}});
  }
  return std::vector<Measurement>{};
}

}  // namespace spectator
