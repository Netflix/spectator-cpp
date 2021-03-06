#pragma once

#include "id.h"
#include "measurement.h"
#include <chrono>
#include <vector>

namespace spectator {

enum class MeterType {
  Counter,
  DistributionSummary,
  Gauge,
  MaxGauge,
  MonotonicCounter,
  Timer
};

inline std::ostream& operator<<(std::ostream& os, const MeterType& mt) {
  switch (mt) {
    case MeterType::Counter:
      os << "Counter";
      break;
    case MeterType::DistributionSummary:
      os << "DistributionSummary";
      break;
    case MeterType::Gauge:
      os << "Gauge";
      break;
    case MeterType::MaxGauge:
      os << "MaxGauge";
      break;
    case MeterType::MonotonicCounter:
      os << "MonotonicCounter";
      break;
    case MeterType::Timer:
      os << "Timer";
      break;
  }
  return os;
}

class Meter {
 public:
  virtual ~Meter() = default;
  virtual IdPtr MeterId() const noexcept = 0;
  virtual std::vector<Measurement> Measure() const noexcept = 0;
  virtual MeterType GetType() const noexcept = 0;
  std::chrono::system_clock::time_point Updated() const noexcept {
    return last_updated_;
  }

 private:
  std::chrono::system_clock::time_point last_updated_{};

 protected:
  void Update() { last_updated_ = std::chrono::system_clock::now(); }
};

}  // namespace spectator
