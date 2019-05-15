#pragma once

#include "id.h"
#include "measurement.h"
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
};

}  // namespace spectator
