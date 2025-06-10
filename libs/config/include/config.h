#pragma once

#include <string>
#include <unordered_map>

#include <libs/writer/writer_config/include/writer_config.h>

class Config
{
  public:

    Config(WriterConfig writerConfig, const std::unordered_map<std::string, std::string> &extra_tags = {});

    // Rule of 5
    ~Config()                                  = default; // Destructor
    Config(const Config &other)                = default; // Copy constructor
    Config &operator=(const Config &other)     = delete; // Copy assignment operator
    Config(Config &&other) noexcept            = delete; // Move constructor
    Config &operator=(Config &&other) noexcept = delete; // Move assignment operator

    const std::string &GetLocation() const { return m_writerConfig.GetLocation(); }

    const std::unordered_map<std::string, std::string> &GetExtraTags() const{ return m_extraTags;}

    const WriterType &GetWriterType() const { return m_writerConfig.GetType();}

  private:
    std::unordered_map<std::string, std::string> CalculateTags(const std::unordered_map<std::string, std::string> &tags);
    
    std::unordered_map<std::string, std::string> m_extraTags;
    WriterConfig m_writerConfig;
};