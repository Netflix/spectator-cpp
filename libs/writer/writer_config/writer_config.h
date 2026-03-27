#pragma once

#include <writer_types.h>

#include <string>
#include <stdexcept>

namespace spectator {

class WriterConfig
{
   public:
    explicit WriterConfig(const std::string& type);
    WriterConfig(const std::string& type, unsigned int bufferSize);
    WriterConfig(const std::string& type, unsigned int bufferSize, unsigned int flushIntervalMs);

    [[nodiscard]] const WriterType& GetType() const noexcept { return m_type; }
    [[nodiscard]] unsigned int GetBufferSize() const noexcept { return m_bufferSize; }
    [[nodiscard]] const std::string& GetLocation() const noexcept { return m_location; }
    [[nodiscard]] unsigned int GetFlushIntervalMs() const noexcept { return m_flushIntervalMs; }

   private:
    WriterType m_type;
    std::string m_location;
    unsigned int m_bufferSize = 0;
    unsigned int m_flushIntervalMs = 0;
};

}  // namespace spectator