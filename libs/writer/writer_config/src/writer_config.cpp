#include <libs/writer/writer_config/include/writer_config.h>

struct WriterConfigConstants
{
    static constexpr auto RuntimeErrorMessage = "Invalid writer type: ";
};

std::pair<WriterType, std::string> GetWriterConfigFromString(const std::string &type)
{
    // Check exact matches first
    auto it = TypeToLocationMap.find(type);
    if (it != TypeToLocationMap.end())
    {
        return {it->second.first, std::string(it->second.second)};
    }

    if (type.rfind(WriterTypes::UDPURL, 0) == 0)
    {
        return {WriterType::UDP, type};
    }
    
    if (type.rfind(WriterTypes::UnixURL, 0) == 0)
    {
        return {WriterType::Unix, type};
    }

    throw std::runtime_error(WriterConfigConstants::RuntimeErrorMessage + type);
}

WriterConfig::WriterConfig(const std::string &type)
{
    const char *envLocation = std::getenv("SPECTATOR_OUTPUT_LOCATION");
    if (envLocation != nullptr)
    {
        Logger::debug("Using environment variable SPECTATOR_OUTPUT_LOCATION: {}", envLocation);
        std::string envValue(envLocation);
        auto [writer_type, location] = GetWriterConfigFromString(envValue);
        m_type                       = writer_type;
        m_location                   = location;
    }
    else
    {
        Logger::debug("Using provided type: {}", type);
        auto [writer_type, location] = GetWriterConfigFromString(type);
        m_type                       = writer_type;
        m_location                   = location;
    }
    Logger::debug("WriterConfig initialized with type: {}, location: {}", WriterTypeToString(m_type), m_location);   
}