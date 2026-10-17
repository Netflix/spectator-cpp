#include <config.h>

#include <writer_types.h>
#include <logger.h>
#include <util.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <initializer_list>

namespace spectator {

struct ConfigConstants
{
    static constexpr auto Container = "nf.container";
    static constexpr auto Process = "nf.process";
    static constexpr auto EnvVarContainer = "NETFLIX_CONTAINER_NAME";
    static constexpr auto EnvVarContainerFallback = "TITUS_CONTAINER_NAME";
    static constexpr auto EnvVarProcess = "NETFLIX_PROCESS_NAME";
    // Deprecated: read only when NETFLIX_PROCESS_NAME is unset or blank. Set NETFLIX_PROCESS_NAME instead.
    static constexpr auto EnvVarProcessFallback = "TITUS_PROCESS_NAME";
};

// Sets tags[tag] from the first of env_vars that is set and not empty or whitespace-only. The value is stored as-is
// (not trimmed).
static void AddFirstNonEmpty(std::unordered_map<std::string, std::string>& tags, const char* tag,
                             std::initializer_list<const char*> env_vars)
{
    for (const char* env_var : env_vars)
    {
        const char* value = std::getenv(env_var);
        if (value != nullptr && IsEmptyOrWhitespace(value) == false)
        {
            tags[tag] = value;
            return;
        }
    }
}

std::unordered_map<std::string, std::string> CalculateTags(
    const std::unordered_map<std::string, std::string>& tags, bool includeEnvTags)
{
    std::unordered_map<std::string, std::string> valid_tags;

    for (const auto& [fst, snd] : tags)
    {
        if (IsEmptyOrWhitespace(fst) == false && IsEmptyOrWhitespace(snd) == false)
        {
            valid_tags[fst] = snd;
        }
    }

    // Added after the passed-in tags, so the environment values win on a key collision.
    if (includeEnvTags == true)
    {
        AddFirstNonEmpty(valid_tags, ConfigConstants::Container,
                         {ConfigConstants::EnvVarContainer, ConfigConstants::EnvVarContainerFallback});
        AddFirstNonEmpty(valid_tags, ConfigConstants::Process,
                         {ConfigConstants::EnvVarProcess, ConfigConstants::EnvVarProcessFallback});
    }

    return valid_tags;
}

Config::Config(const WriterConfig& writerConfig, const std::unordered_map<std::string, std::string>& extraTags,
               bool includeEnvTags)
    : m_extraTags(CalculateTags(extraTags, includeEnvTags)), m_writerConfig(writerConfig)
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

}  // namespace spectator
