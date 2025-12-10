#include <writer_config.h>

#include <logger.h>

namespace spectator {

struct WriterConfigConstants
{
    static constexpr auto RuntimeErrorMessage = "Invalid writer type: ";
};

std::pair<WriterType, std::string> GetWriterConfigFromString(const std::string& type)
{
    // Check exact matches first
    if (const auto it = TypeToLocationMap.find(type); it != TypeToLocationMap.end())
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

WriterConfig::WriterConfig(const std::string& type)
{
    if (const char* envLocation = std::getenv("SPECTATOR_OUTPUT_LOCATION"); envLocation != nullptr)
    {
        Logger::info("Environment variable set, SPECTATOR_OUTPUT_LOCATION: {}", envLocation);
        const std::string envValue(envLocation);
        auto [writer_type, location] = GetWriterConfigFromString(envValue);
        m_type = writer_type;
        m_location = location;
    }
    else
    {
        auto [writer_type, location] = GetWriterConfigFromString(type);
        m_type = writer_type;
        m_location = location;
    }
    Logger::info("WriterConfig initialized with type: {}, location: {}", WriterTypeToString(m_type), m_location);
}

WriterConfig::WriterConfig(const std::string& type, const unsigned int bufferSize)
    : WriterConfig(type)  // Constructor delegation
{
    m_bufferSize = bufferSize;
    Logger::info("WriterConfig buffering enabled with size: {}", m_bufferSize);
}

}  // namespace spectator