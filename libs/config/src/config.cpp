#include <libs/config/include/config.h>

struct ConfigConstants
{
    static constexpr auto container       = "nf.container";
    static constexpr auto process         = "nf.process";
    static constexpr auto envVarContainer = "TITUS_CONTAINER_NAME";
    static constexpr auto envVarProcess   = "TITUS_PROCESS_NAME";
};

Config::Config(WriterConfig writerConfig, const std::unordered_map<std::string, std::string> &extra_tags)
    : m_extraTags(CalculateTags(extra_tags)), m_writerConfig(writerConfig) {}

std::unordered_map<std::string, std::string> Config::CalculateTags(const std::unordered_map<std::string, std::string> &tags)
{
    std::unordered_map<std::string, std::string> valid_tags;

    const char *container_name = std::getenv(ConfigConstants::envVarContainer);
    const char *process_name   = std::getenv(ConfigConstants::envVarProcess);
    if (container_name)
    {
        valid_tags[ConfigConstants::container] = container_name;
    }
    
    if (process_name)
    {
        valid_tags[ConfigConstants::process] = process_name;
    }

    for (const auto &kv : tags)
    {
        if (!kv.first.empty() && !kv.second.empty())
        {
            valid_tags[kv.first] = kv.second;
        }
    }

    return valid_tags;
}
