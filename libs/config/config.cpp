#include <libs/config/config.h>
#include <algorithm>
#include <cctype>

struct ConfigConstants
{
    static constexpr auto container = "nf.container";
    static constexpr auto process = "nf.process";
    static constexpr auto envVarContainer = "TITUS_CONTAINER_NAME";
    static constexpr auto envVarProcess = "TITUS_PROCESS_NAME";
};

bool IsEmptyOrWhitespace(const std::string& str)
{
    return str.empty() || std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isspace(c); });
}

std::unordered_map<std::string, std::string> CalculateTags(
    const std::unordered_map<std::string, std::string>& tags)
{
    std::unordered_map<std::string, std::string> valid_tags;

    const char* container_name = std::getenv(ConfigConstants::envVarContainer);
    const char* process_name = std::getenv(ConfigConstants::envVarProcess);
    
    if (container_name != nullptr)
    {
        std::string container_str(container_name);
        if (IsEmptyOrWhitespace(container_str) == false)
        {
            valid_tags[ConfigConstants::container] = container_str;
        }
    }

    if (process_name != nullptr)
    {
        std::string process_str(process_name);
        if (IsEmptyOrWhitespace(process_str) == false)
        {
            valid_tags[ConfigConstants::process] = process_str;
        }
    }

    for (const auto& [fst, snd] : tags)
    {
        if (IsEmptyOrWhitespace(fst) == false && IsEmptyOrWhitespace(snd) == false)
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
