#pragma once

#include <string>
#include <unordered_map>

#include <writer_config.h>

namespace spectator {

class Config
{
   public:
    // When includeEnvTags is true (the default), nf.container and nf.process are added from the environment and win
    // over extraTags (see README.md). Pass false in a process that reports metrics on behalf of other containers or
    // processes, such as a relay or an agent like atlas-system-agent's pod monitor, so its own identity does not
    // overwrite their per-meter tags.
    Config(const WriterConfig& writerConfig, const std::unordered_map<std::string, std::string>& extraTags = {},
           bool includeEnvTags = true);

    ~Config() = default;
    Config(const Config& other) = default;
    Config& operator=(const Config& other) = delete;
    Config(Config&& other) = delete;
    Config& operator=(Config&& other) = delete;

    const std::unordered_map<std::string, std::string>& GetExtraTags() const noexcept { return m_extraTags; }

    const std::string& GetWriterLocation() const noexcept { return m_writerConfig.GetLocation(); }
    const WriterType& GetWriterType() const noexcept { return m_writerConfig.GetType(); }
    const unsigned int GetWriterBufferSize() const noexcept { return m_writerConfig.GetBufferSize(); }
    const BufferConfig& GetWriterBufferConfig() const noexcept { return m_writerConfig.GetBufferConfig(); }


   private:
    std::unordered_map<std::string, std::string> m_extraTags;
    WriterConfig m_writerConfig;
};

}  // namespace spectator