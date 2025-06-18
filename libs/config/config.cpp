#include <libs/config/config.h>

struct ConfigConstants
{
    static constexpr auto container = "nf.container";
    static constexpr auto process = "nf.process";
    static constexpr auto envVarContainer = "TITUS_CONTAINER_NAME";
    static constexpr auto envVarProcess = "TITUS_PROCESS_NAME";
};

std::unordered_map<std::string, std::string> CalculateTags(
    const std::unordered_map<std::string, std::string>& tags)
{
    std::unordered_map<std::string, std::string> valid_tags;

    const char* container_name = std::getenv(ConfigConstants::envVarContainer);
    const char* process_name = std::getenv(ConfigConstants::envVarProcess);
    if (container_name != nullptr)
    {
        valid_tags[ConfigConstants::container] = container_name;
    }

    if (process_name != nullptr)
    {
        valid_tags[ConfigConstants::process] = process_name;
    }

    for (const auto& [fst, snd] : tags)
    {
        if (fst.empty() == false && snd.empty() == false)
        {
            valid_tags[fst] = snd;
        }
    }

    return valid_tags;
}

Config::Config(const WriterConfig& writerConfig, const std::unordered_map<std::string, std::string>& extraTags)
    : m_extraTags(CalculateTags(extraTags)), m_writerConfig(writerConfig)
{
}
