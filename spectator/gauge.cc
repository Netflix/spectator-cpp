#include "gauge.h"
#include <cmath>

namespace spectator {

static constexpr auto kNAN = std::numeric_limits<double>::quiet_NaN();

Gauge::Gauge(IdPtr id) noexcept : id_{std::move(id)}, value_{kNAN} {}

IdPtr Gauge::MeterId() const noexcept { return id_; }
std::vector<Measurement> Gauge::Measure() const noexcept {
  auto value = value_.exchange(kNAN, std::memory_order_relaxed);
  if (std::isnan(value)) {
    return std::vector<Measurement>();
  }
  return std::vector<Measurement>({{id_->WithStat("gauge"), value}});
}

void Gauge::Set(double value) noexcept {
  value_.store(value, std::memory_order_relaxed);
}

double Gauge::Get() const noexcept {
  return value_.load(std::memory_order_relaxed);
}

}  // namespace spectator
