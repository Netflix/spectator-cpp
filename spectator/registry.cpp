#include <registry.h>

namespace spectator {


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
        Writer::Initialize(config.GetWriterType(), "", 0, this->m_config.GetWriterBufferSize()); 
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

MeterId Registry::CreateNewId(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    MeterId new_meter_id(name, tags);

    if (this->m_config.GetExtraTags().empty() == true)
    {
        return new_meter_id;
    }
    return new_meter_id.WithTags(this->m_config.GetExtraTags());
}

AgeGauge Registry::CreateAgeGauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return AgeGauge(CreateNewId(name, tags));
}

AgeGauge Registry::CreateAgeGauge(const MeterId& meter_id) const { return AgeGauge(meter_id); }

Counter Registry::CreateCounter(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return Counter(CreateNewId(name, tags));
}

Counter Registry::CreateCounter(const MeterId& meter_id) const { return Counter(meter_id); }

DistributionSummary Registry::CreateDistributionSummary(const std::string& name,
                                                   const std::unordered_map<std::string, std::string>& tags) const
{
    return DistributionSummary(CreateNewId(name, tags));
}

DistributionSummary Registry::CreateDistributionSummary(const MeterId& meter_id) const
{
    return DistributionSummary(meter_id);
}

Gauge Registry::CreateGauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags,
                      const std::optional<int>& ttl_seconds) const
{
    return Gauge(CreateNewId(name, tags), ttl_seconds);
}

Gauge Registry::CreateGauge(const MeterId& meter_id, const std::optional<int>& ttl_seconds) const
{
    return Gauge(meter_id, ttl_seconds);
}

MaxGauge Registry::CreateMaxGauge(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return MaxGauge(CreateNewId(name, tags));
}

MaxGauge Registry::CreateMaxGauge(const MeterId& meter_id) const { return MaxGauge(meter_id); }

MonotonicCounter Registry::CreateMonotonicCounter(const std::string& name,
                                             const std::unordered_map<std::string, std::string>& tags) const
{
    return MonotonicCounter(CreateNewId(name, tags));
}

MonotonicCounter Registry::CreateMonotonicCounter(const MeterId& meter_id) const { return MonotonicCounter(meter_id); }

MonotonicCounterUint Registry::CreateMonotonicCounterUint(const std::string& name,
                                                      const std::unordered_map<std::string, std::string>& tags) const
{
    return MonotonicCounterUint(CreateNewId(name, tags));
}

MonotonicCounterUint Registry::CreateMonotonicCounterUint(const MeterId& meter_id) const
{
    return MonotonicCounterUint(meter_id);
}

PercentileDistributionSummary Registry::CreatePercentDistributionSummary(
    const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return PercentileDistributionSummary(CreateNewId(name, tags));
}

PercentileDistributionSummary Registry::CreatePercentDistributionSummary(const MeterId& meter_id) const
{
    return PercentileDistributionSummary(meter_id);
}

PercentileTimer Registry::CreatePercentTimer(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return PercentileTimer(CreateNewId(name, tags));
}

PercentileTimer Registry::CreatePercentTimer(const MeterId& meter_id) const { return PercentileTimer(meter_id); }

Timer Registry::CreateTimer(const std::string& name, const std::unordered_map<std::string, std::string>& tags) const
{
    return Timer(CreateNewId(name, tags));
}

Timer Registry::CreateTimer(const MeterId& meter_id) const { return Timer(meter_id); }

}  // namespace spectator