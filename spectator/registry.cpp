#include <spectator/registry.h>

Registry::Registry(const Config& config) : m_config(config) { Writer::Initialize(config.GetWriterType()); }

Registry::~Registry()
{
    // No need to close Writer here as it's a singleton
    // and will live beyond Registry instances
}

MeterId Registry::new_id(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    MeterId new_meter_id(name, tags);

    if (this->m_config.GetExtraTags().empty() == true)
    {
        return new_meter_id;
    }
    else
    {
        return new_meter_id.WithTags(this->m_config.GetExtraTags());
    }
}

AgeGauge Registry::age_gauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return AgeGauge(new_id(name, tags));
}

AgeGauge Registry::age_gauge_with_id(const MeterId& meter_id) { return AgeGauge(meter_id); }

Counter Registry::counter(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return Counter(new_id(name, tags));
}

Counter Registry::counter_with_id(const MeterId& meter_id) { return Counter(meter_id); }

DistributionSummary Registry::distribution_summary(const std::string& name,
                                                   const std::unordered_map<std::string, std::string>& tags) const
{
    return DistributionSummary(new_id(name, tags));
}

DistributionSummary Registry::distribution_summary_with_id(const MeterId& meter_id)
{
    return DistributionSummary(meter_id);
}

Gauge Registry::gauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags,
                      const std::optional<int>& ttl_seconds) const
{
    return Gauge(new_id(name, tags), ttl_seconds);
}

Gauge Registry::gauge_with_id(const MeterId& meter_id, const std::optional<int>& ttl_seconds)
{
    return Gauge(meter_id, ttl_seconds);
}

MaxGauge Registry::max_gauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return MaxGauge(new_id(name, tags));
}

MaxGauge Registry::max_gauge_with_id(const MeterId& meter_id) { return MaxGauge(meter_id); }

MonotonicCounter Registry::monotonic_counter(const std::string& name,
                                             const std::unordered_map<std::string, std::string>& tags) const
{
    return MonotonicCounter(new_id(name, tags));
}

MonotonicCounter Registry::monotonic_counter_with_id(const MeterId& meter_id) { return MonotonicCounter(meter_id); }

MonotonicCounterUint Registry::monotonic_counter_uint(const std::string& name,
                                                      const std::unordered_map<std::string, std::string>& tags) const
{
    return MonotonicCounterUint(new_id(name, tags));
}

MonotonicCounterUint Registry::monotonic_counter_uint_with_id(const MeterId& meter_id)
{
    return MonotonicCounterUint(meter_id);
}

PercentileDistributionSummary Registry::pct_distribution_summary(
    const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return PercentileDistributionSummary(new_id(name, tags));
}

PercentileDistributionSummary Registry::pct_distribution_summary_with_id(const MeterId& meter_id)
{
    return PercentileDistributionSummary(meter_id);
}

PercentileTimer Registry::pct_timer(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return PercentileTimer(new_id(name, tags));
}

PercentileTimer Registry::pct_timer_with_id(const MeterId& meter_id) { return PercentileTimer(meter_id); }

Timer Registry::timer(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return Timer(new_id(name, tags));
}

Timer Registry::timer_with_id(const MeterId& meter_id) { return Timer(meter_id); }