#pragma once

#include "meter.h"
#include <atomic>

namespace spectator {
class MonotonicCounter : public Meter {
 public:
  explicit MonotonicCounter(IdPtr id) noexcept;
  IdPtr MeterId() const noexcept override;
  std::vector<Measurement> Measure() const noexcept override;
  MeterType GetType() const noexcept override {
    return MeterType::MonotonicCounter;
  }

  void Set(double amount) noexcept;
  double Delta() const noexcept;

 private:
  IdPtr id_;
  mutable std::atomic<double> value_;
  mutable std::atomic<double> prev_value_;
};
}  // namespace spectator
