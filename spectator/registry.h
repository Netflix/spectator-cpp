#pragma once

#include <config.h>
#include <meter_id.h>
#include <meter_types.h>
#include <writer.h>

// Only for spdlog::level::level_enum. Deliberately not <logger.h>, which would
// also drag in spdlog/async.h, fmt, and the spectator::Logger name.
#include <spdlog/common.h>

#include <memory>
#include <string>
#include <map>
#include <optional>
#include <unordered_map>
#include <type_traits>

namespace spectator {

// Sets the severity threshold of the spectator logger. Lets callers adjust
// logging without including <logger.h>, so spectator::Logger stays out of
// consumers' namespaces. No-op if the logger failed to initialize.
void SetLogLevel(spdlog::level::level_enum level);

class Registry
{
   public:
    explicit Registry(const Config& config);
    ~Registry() = default;

    MeterId CreateNewId(const std::string& name, const std::unordered_map<std::string, std::string>& tags = {}) const;

    AgeGauge CreateAgeGauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                    std::unordered_map<std::string, std::string>()) const;

    AgeGauge CreateAgeGauge(const MeterId& meter_id) const;

    Counter CreateCounter(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                 std::unordered_map<std::string, std::string>()) const;

    Counter CreateCounter(const MeterId& meter_id) const;

    DistributionSummary CreateDistributionSummary(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>()) const;

    DistributionSummary CreateDistributionSummary(const MeterId& meter_id) const;

    Gauge CreateGauge(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>(),
        const std::optional<int>& ttl_seconds = std::nullopt) const;

    Gauge CreateGauge(const MeterId& meter_id, const std::optional<int>& ttl_seconds = std::nullopt) const;

    MaxGauge CreateMaxGauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                    std::unordered_map<std::string, std::string>()) const;

    MaxGauge CreateMaxGauge(const MeterId& meter_id) const;

    MonotonicCounter CreateMonotonicCounter(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>()) const;

    MonotonicCounter CreateMonotonicCounter(const MeterId& meter_id) const;

    MonotonicCounterUint CreateMonotonicCounterUint(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>()) const;

    MonotonicCounterUint CreateMonotonicCounterUint(const MeterId& meter_id) const;

    PercentileDistributionSummary CreatePercentDistributionSummary(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>()) const;

    PercentileDistributionSummary CreatePercentDistributionSummary(const MeterId& meter_id) const;

    PercentileTimer CreatePercentTimer(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                           std::unordered_map<std::string, std::string>()) const;

    PercentileTimer CreatePercentTimer(const MeterId& meter_id) const;

    Timer CreateTimer(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                             std::unordered_map<std::string, std::string>()) const;

    Timer CreateTimer(const MeterId& meter_id) const;

   private:
    Config m_config;
};

}  // namespace spectator