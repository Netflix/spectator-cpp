#pragma once
#include "meter.h"
#include <atomic>
#include <chrono>

namespace spectator {

class Timer : public Meter {
 public:
  explicit Timer(IdPtr id) noexcept;
  IdPtr MeterId() const noexcept override;
  std::vector<Measurement> Measure() const noexcept override;
  MeterType GetType() const noexcept override { return MeterType::Timer; }

  void Record(std::chrono::nanoseconds amount) noexcept;
  int64_t Count() const noexcept;
  int64_t TotalTime() const noexcept;

 private:
  IdPtr id_;
  mutable std::atomic<int64_t> count_;
  mutable std::atomic<int64_t> total_;
  mutable std::atomic<double> totalSq_;
  mutable std::atomic<int64_t> max_;
};

}  // namespace spectator
