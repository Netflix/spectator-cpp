#pragma once

#include <fmt/format.h>

namespace spectator {
enum class MeterType {
  AgeGauge,
  Counter,
  DistSummary,
  Gauge,
  MaxGauge,
  MonotonicCounter,
  MonotonicCounterUint,
  PercentileDistSummary,
  PercentileTimer,
  Timer
};
}

template <> struct fmt::formatter<spectator::MeterType>: formatter<std::string_view> {
  auto format(spectator::MeterType meter_type, format_context& ctx) const -> format_context::iterator {
    using namespace spectator;
    std::string_view s = "unknown";

    switch (meter_type) {
      case MeterType::AgeGauge:
        s = "age-gauge";
        break;
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
      case MeterType::MonotonicCounter:
        s = "monotonic-counter";
        break;
      case MeterType::MonotonicCounterUint:
        s = "monotonic-counter-uint";
        break;
      case MeterType::PercentileDistSummary:
        s = "percentile-distribution-summary";
        break;
      case MeterType::PercentileTimer:
        s = "percentile-timer";
        break;
      case MeterType::Timer:
        s = "timer";
        break;
    }

    return fmt::formatter<std::string_view>::format(s, ctx);
  }
};
