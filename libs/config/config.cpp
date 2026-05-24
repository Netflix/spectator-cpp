#include <config.h>

#include <writer_types.h>
#include <logger.h>
#include <util.h>

#include <algorithm>
#include <cctype>

namespace spectator {

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
    std::unordered_map<std::string, std::string> valid_tags = ValidateTags(tags);

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
    : m_extraTags(std::make_shared<ExtraCommonTags>()), m_writerConfig(writerConfig)
{
    ExtraCommonTags ect;
    ect.tags = CalculateTags(extraTags);

    bool first = true;
    for (const auto& [key, val] : ect.tags)
    {
        if (!first) ect.prefixString += ',';
        ect.prefixString += key;
        ect.prefixString += '=';
        ect.prefixString += val;
        first = false;
    }

    Logger::info("Config initialized with writer type: {}", WriterTypeToString(m_writerConfig.GetType()));

    if (ect.tags.empty())
    {
        Logger::info("Config initialized with no extra tags.");
    }
    else
    {
        Logger::info("Config initialized with the following extra tags:");
        for (const auto& [key, value] : ect.tags)
        {
            Logger::info("  {}: {}", key, value);
        }
    }

    m_extraTags = std::make_shared<const ExtraCommonTags>(std::move(ect));
}

}  // namespace spectator
