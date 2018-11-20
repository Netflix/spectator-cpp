#pragma once

#include "meter.h"
#include <atomic>

namespace spectator {

class MaxGauge : public Meter {
 public:
  explicit MaxGauge(IdPtr id) noexcept;
  IdPtr MeterId() const noexcept override;
  std::vector<Measurement> Measure() const noexcept override;
  MeterType GetType() const noexcept override { return MeterType::MaxGauge; };

  void Update(double value) noexcept;

  // synonym for Update for consistency with the Gauge interface
  void Set(double value) noexcept { Update(value); }
  double Get() const noexcept;

 private:
  IdPtr id_;
  mutable std::atomic<double> value_;
};

}  // namespace spectator
