#pragma once

#include <config.h>
#include <logger.h>
#include <meter_id.h>
#include <meter_types.h>
#include <writer.h>

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
    ~Registry() = default;

    MeterId CreateNewId(const std::string& name, const std::unordered_map<std::string, std::string>& tags = {}) const;

    AgeGauge CreateAgeGauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                    std::unordered_map<std::string, std::string>()) const;

    static AgeGauge CreateAgeGauge(const MeterId& meter_id);

    Counter CreateCounter(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                 std::unordered_map<std::string, std::string>()) const;

    static Counter CreateCounter(const MeterId& meter_id);

    DistributionSummary CreateDistributionSummary(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>()) const;

    static DistributionSummary CreateDistributionSummary(const MeterId& meter_id);

    Gauge CreateGauge(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>(),
        const std::optional<int>& ttl_seconds = std::nullopt) const;

    static Gauge CreateGauge(const MeterId& meter_id, const std::optional<int>& ttl_seconds = std::nullopt);

    MaxGauge CreateMaxGauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                    std::unordered_map<std::string, std::string>()) const;

    static MaxGauge CreateMaxGauge(const MeterId& meter_id);

    MonotonicCounter CreateMonotonicCounter(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>()) const;

    static MonotonicCounter CreateMonotonicCounter(const MeterId& meter_id);

    MonotonicCounterUint CreateMonotonicCounterUint(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>()) const;

    static MonotonicCounterUint CreateMonotonicCounterUint(const MeterId& meter_id);

    PercentileDistributionSummary CreatePercentDistributionSummary(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& tags = std::unordered_map<std::string, std::string>()) const;

    static PercentileDistributionSummary CreatePercentDistributionSummary(const MeterId& meter_id);

    PercentileTimer CreatePercentTimer(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                                           std::unordered_map<std::string, std::string>()) const;

    static PercentileTimer CreatePercentTimer(const MeterId& meter_id);

    Timer CreateTimer(const std::string& name, const std::unordered_map<std::string, std::string>& tags =
                                             std::unordered_map<std::string, std::string>()) const;

    static Timer CreateTimer(const MeterId& meter_id);

   private:
    Config m_config;
};