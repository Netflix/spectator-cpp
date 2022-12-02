#pragma once

#include <fmt/format.h>

namespace spectator {
enum class MeterType {
  Counter,
  DistSummary,
  Gauge,
  MaxGauge,
  AgeGauge,
  MonotonicCounter,
  Timer,
  PercentileTimer,
  PercentileDistSummary
};

}

// Provide a formatter for MeterType
template <>
struct fmt::formatter<spectator::MeterType> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const spectator::MeterType& meter_type, FormatContext& context) {
    using namespace spectator;
    std::string_view s = "unknown";
    switch (meter_type) {
      case MeterType::Counter:
        s = "counter";
        break;
      case MeterType::DistSummary:
        s = "distribution-summary";
        break;
      case MeterType::Gauge:
        s = "gauge";
        break;
      case MeterType::MaxGauge:
        s = "max-gauge";
        break;
      case MeterType::AgeGauge:
        s = "age-gauge";
        break;
      case MeterType::MonotonicCounter:
        s = "monotonic-counter";
        break;
      case MeterType::Timer:
        s = "timer";
        break;
      case MeterType::PercentileTimer:
        s = "percentile-timer";
        break;
      case MeterType::PercentileDistSummary:
        s = "percentile-distribution-summary";
        break;
    }
    return fmt::formatter<std::string_view>::format(s, context);
  }
};
