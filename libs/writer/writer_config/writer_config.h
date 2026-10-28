#pragma once

#include <buffer_config.h>
#include <writer_types.h>

#include <chrono>
#include <string>
#include <stdexcept>

namespace spectator {

class WriterConfig
{
   public:
    explicit WriterConfig(const std::string& type);

    // Buffers up to bufferSize bytes per writing thread, with the default flush interval and backpressure.
    WriterConfig(const std::string& type, unsigned int bufferSize);

    // Full control over buffering. Throws std::invalid_argument if the flush interval is not positive.
    WriterConfig(const std::string& type, const BufferConfig& bufferConfig);

    [[nodiscard]] const WriterType& GetType() const noexcept { return m_type; }
    [[nodiscard]] const std::string& GetLocation() const noexcept { return m_location; }
    [[nodiscard]] const BufferConfig& GetBufferConfig() const noexcept { return m_bufferConfig; }
    [[nodiscard]] unsigned int GetBufferSize() const noexcept { return m_bufferConfig.size; }
    [[nodiscard]] std::chrono::milliseconds GetFlushInterval() const noexcept { return m_bufferConfig.flushInterval; }
    [[nodiscard]] bool GetDropOnFull() const noexcept { return m_bufferConfig.dropOnFull; }

   private:
    WriterType m_type;
    std::string m_location;
    BufferConfig m_bufferConfig{};
};

}  // namespace spectator
