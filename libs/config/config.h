#pragma once

#include <string>
#include <unordered_map>

#include <libs/writer/writer_config/writer_config.h>

class Config
{
   public:
    Config(const WriterConfig& writerConfig, const std::unordered_map<std::string, std::string>& extraTags = {});

    ~Config() = default;
    Config(const Config& other) = default;
    Config& operator=(const Config& other) = delete;
    Config(Config&& other) noexcept = delete;
    Config& operator=(Config&& other) noexcept = delete;

    const std::unordered_map<std::string, std::string>& GetExtraTags() const noexcept { return m_extraTags; }

    const std::string& GetWriterLocation() const noexcept { return m_writerConfig.GetLocation(); }
    const WriterType& GetWriterType() const noexcept { return m_writerConfig.GetType(); }
    const unsigned int GetWriterBufferSize() const noexcept { return m_writerConfig.GetBufferSize(); }


   private:
    std::unordered_map<std::string, std::string> m_extraTags;
    WriterConfig m_writerConfig;
};