#include <config.h>

#include <writer_types.h>
#include <logger.h>
#include <util.h>

#include <algorithm>
#include <cctype>

struct ConfigConstants
{
    static constexpr auto Container = "nf.container";
    static constexpr auto Process = "nf.process";
    static constexpr auto EnvVarContainer = "TITUS_CONTAINER_NAME";
    static constexpr auto EnvVarProcess = "TITUS_PROCESS_NAME";
};

std::unordered_map<std::string, std::string> CalculateTags(
    const std::unordered_map<std::string, std::string>& tags)
{
    std::unordered_map<std::string, std::string> valid_tags;

    for (const auto& [fst, snd] : tags)
    {
        if (IsEmptyOrWhitespace(fst) == false && IsEmptyOrWhitespace(snd) == false)
        {
            valid_tags[fst] = snd;
        }
    }

    const char* container_name = std::getenv(ConfigConstants::EnvVarContainer);
    const char* process_name = std::getenv(ConfigConstants::EnvVarProcess);

    if (container_name != nullptr)
    {
        std::string container_str(container_name);
        if (IsEmptyOrWhitespace(container_str) == false)
        {
            valid_tags[ConfigConstants::Container] = container_str;
        }
    }

    if (process_name != nullptr)
    {
        std::string process_str(process_name);
        if (IsEmptyOrWhitespace(process_str) == false)
        {
            valid_tags[ConfigConstants::Process] = process_str;
        }
    }

    return valid_tags;
}

Config::Config(const WriterConfig& writerConfig, const std::unordered_map<std::string, std::string>& extraTags)
    : m_extraTags(CalculateTags(extraTags)), m_writerConfig(writerConfig)
{
    Logger::info("Config initialized with writer type: {}, buffer size: {}, location: {}",
                     WriterTypeToString(m_writerConfig.GetType()), m_writerConfig.GetBufferSize(), m_writerConfig.GetLocation());
    if (m_extraTags.empty() == true)
    {
        Logger::info("Config initialized with no extra tags provided.");
        return;
    }

    Logger::info("Config initialized with the following extra tags:");
    for (const auto& [key, value] : m_extraTags)
    {
        Logger::info("  {}: {}", key, value);
    }
}
