#include <libs/writer/writer_config/include/writer_config.h>

std::pair<WriterType, std::string> GetWriterConfigFromString(const std::string &type)
{
    // Check exact matches first
    auto it = TypeToLocationMap.find(type);
    if (it != TypeToLocationMap.end())
    {
        return {it->second.first, std::string(it->second.second)};
    }

    else if (type.rfind(WriterTypes::UDPURL, 0) == 0)
    {
        return {WriterType::UDP, type};
    }
    else if (type.rfind(WriterTypes::UnixURL, 0) == 0)
    {
        return {WriterType::Unix, type};
    }

    throw std::invalid_argument("Invalid writer type: " + type);
}



WriterConfig::WriterConfig(std::string type)
{
    const char *envLocation = std::getenv("SPECTATOR_OUTPUT_LOCATION");

    try
    {
        if (envLocation != nullptr)
        {
            // If environment variable is set, use it instead of the constructor parameter
            std::string envValue(envLocation);
            auto [writer_type, location] = GetWriterConfigFromString(envValue);
            m_type                       = writer_type;
            m_location                   = location;
        }
        else
        {
            // If no environment variable, use the type passed to the constructor
            auto [writer_type, location] = GetWriterConfigFromString(type);
            m_type                       = writer_type;
            m_location                   = location;
        }
    }
    catch (const std::invalid_argument &e)
    {
        throw std::invalid_argument("Invalid writer type: " + (envLocation != nullptr ? std::string(envLocation) : type));
    }
}