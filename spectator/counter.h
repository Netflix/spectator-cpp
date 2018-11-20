#pragma once

#include "meter.h"
#include <atomic>

namespace spectator {

class Counter : public Meter {
 public:
  explicit Counter(IdPtr id) noexcept;
  IdPtr MeterId() const noexcept override;
  std::vector<Measurement> Measure() const noexcept override;
  MeterType GetType() const noexcept override { return MeterType::Counter; }

  void Increment() noexcept;
  void Add(double delta) noexcept;
  double Count() const noexcept;

 private:
  IdPtr id_;
  mutable std::atomic<double> count_;
};

}  // namespace spectator
