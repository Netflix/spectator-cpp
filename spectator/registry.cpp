#include <registry.h>


std::pair<std::string, int> ParseUdpAddress(const std::string& address) 
{
    std::regex pattern("udp://([0-9.]+):(\\d+)");
    std::smatch matches;
        
    if (!std::regex_match(address, matches, pattern) || matches.size() != 3)
    {
        throw std::runtime_error("Invalid UDP address format");
    }
            
    std::string ip = matches[1].str();
    int port = std::stoi(matches[2]);
    if (port < 0 || port > 65535)
    {
        throw std::runtime_error("Port number out of valid range (0-65535)");
    }

    return {ip, port};
}

std::string ParseUnixAddress(const std::string& address) 
{
    std::regex pattern("unix://(.*)");
    std::smatch matches;
        
    if (!std::regex_match(address, matches, pattern) || matches.size() != 2)
    {
        throw std::runtime_error("Invalid Unix address format");
    }
            
    return matches[1].str();
}

Registry::Registry(const Config& config) : m_config(config) 
{
    if (config.GetWriterType() == WriterType::Memory)
    {
        Logger::info("Registry initializing Memory Writer");
        Writer::Initialize(config.GetWriterType()); 
    }
    else if (config.GetWriterType() == WriterType::UDP)
    {
        auto [ip, port] = ParseUdpAddress(this->m_config.GetWriterLocation());
        Logger::info("Registry initializing UDP Writer at {}:{}", ip, port);
        Writer::Initialize(config.GetWriterType(), ip, port, this->m_config.GetWriterBufferSize()); 
    }
    else if (config.GetWriterType() == WriterType::Unix)
    {
        auto socketPath = ParseUnixAddress(this->m_config.GetWriterLocation());
        Logger::info("Registry initializing UDS Writer at {}", socketPath);
        Writer::Initialize(config.GetWriterType(), socketPath, 0, this->m_config.GetWriterBufferSize()); 
    }    
}

MeterId Registry::new_id(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    MeterId new_meter_id(name, tags);

    if (this->m_config.GetExtraTags().empty() == true)
    {
        return new_meter_id;
    }
    return new_meter_id.WithTags(this->m_config.GetExtraTags());
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