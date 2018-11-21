#include "max_gauge.h"
#include "atomicnumber.h"

namespace spectator {

static constexpr auto kMinValue = std::numeric_limits<double>::lowest();
static constexpr auto kNaN = std::numeric_limits<double>::quiet_NaN();

MaxGauge::MaxGauge(IdPtr id) noexcept : id_{std::move(id)}, value_{kMinValue} {}

IdPtr MaxGauge::MeterId() const noexcept { return id_; }

std::vector<Measurement> MaxGauge::Measure() const noexcept {
  auto value = value_.exchange(kMinValue, std::memory_order_relaxed);
  if (value == kMinValue) {
    return std::vector<Measurement>();
  }
  return std::vector<Measurement>({{id_->WithStat("max"), value}});
}

double MaxGauge::Get() const noexcept {
  auto v = value_.load(std::memory_order_relaxed);
  if (v != kMinValue) {
    return v;
  }
  return kNaN;
}

void MaxGauge::Update(double value) noexcept { update_max(&value_, value); }

}  // namespace spectator