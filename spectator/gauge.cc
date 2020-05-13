#include "gauge.h"
#include <cmath>

namespace spectator {

static constexpr auto kNAN = std::numeric_limits<double>::quiet_NaN();

Gauge::Gauge(IdPtr id) noexcept : id_{std::move(id)}, value_{kNAN} {}

using std::chrono::system_clock;
inline bool has_expired(system_clock::time_point updated) {
  static constexpr auto kTtl = std::chrono::minutes{15};
  auto ago = system_clock::now() - updated;
  return ago > kTtl;
}

IdPtr Gauge::MeterId() const noexcept { return id_; }
std::vector<Measurement> Gauge::Measure() const noexcept {
  double value;
  if (has_expired(Updated())) {
    value = value_.exchange(kNAN, std::memory_order_relaxed);
  } else {
    value = value_.load(std::memory_order_relaxed);
  }

  if (std::isnan(value)) {
    return std::vector<Measurement>();
  }
  if (!gauge_id_) {
    gauge_id_ = Id::WithDefaultStat(id_, "gauge");
  }
  return std::vector<Measurement>({{gauge_id_, value}});
}

void Gauge::Set(double value) noexcept {
  Update();

  value_.store(value, std::memory_order_relaxed);
}

double Gauge::Get() const noexcept {
  return value_.load(std::memory_order_relaxed);
}

}  // namespace spectator
