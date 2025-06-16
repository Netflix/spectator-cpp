#pragma once

#include <libs/config/config.h>
#include <libs/meter/meter_id/meter_id.h>
#include <libs/meter/meter_types/include/meter_types.h>
#include <libs/writer/writer_wrapper/include/Writer.h>  // Include Writer.h

#include <memory>
#include <string>
#include <map>
#include <optional>
#include <unordered_map>
#include <type_traits>

class Registry
{
   public:
    explicit Registry(const Config& config);
    ~Registry();

    MeterId new_id(const std::string& name, const std::unordered_map<std::string, std::string>& tags = {}) const;

    AgeGauge age_gauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                    std::unordered_map<std::string, std::string>());

    AgeGauge age_gauge_with_id(const MeterId& meter_id);

    Counter counter(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                 std::unordered_map<std::string, std::string>());

    Counter counter_with_id(const MeterId& meter_id);

    DistributionSummary distribution_summary(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>());

    DistributionSummary distribution_summary_with_id(const MeterId& meter_id);

    Gauge gauge(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>(),
        const std::optional<int>& ttl_seconds = std::nullopt);

    Gauge gauge_with_id(const MeterId& meter_id, const std::optional<int>& ttl_seconds = std::nullopt);

    MaxGauge max_gauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                    std::unordered_map<std::string, std::string>());

    MaxGauge max_gauge_with_id(const MeterId& meter_id);

    MonotonicCounter monotonic_counter(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>());

    MonotonicCounter monotonic_counter_with_id(const MeterId& meter_id);

    MonotonicCounterUint monotonic_counter_uint(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>());

    MonotonicCounterUint monotonic_counter_uint_with_id(const MeterId& meter_id);

    PercentileDistributionSummary pct_distribution_summary(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>());

    PercentileDistributionSummary pct_distribution_summary_with_id(const MeterId& meter_id);

    PercentileTimer pct_timer(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                           std::unordered_map<std::string, std::string>());

    PercentileTimer pct_timer_with_id(const MeterId& meter_id);

    Timer timer(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                             std::unordered_map<std::string, std::string>());

    Timer timer_with_id(const MeterId& meter_id);

   private:
    Config m_config;
};