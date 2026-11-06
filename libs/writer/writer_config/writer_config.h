#pragma once

#include <writer_types.h>

#include <chrono>
#include <string>
#include <stdexcept>

namespace spectator {

class WriterConfig
{
   public:
    explicit WriterConfig(const std::string& type);

    // Enables buffering when bufferSize is greater than 0. Each thread that records meters then fills its own buffer
    // of at most bufferSize bytes, and one sender thread sends the buffers.
    //   flushInterval:    how often the sender thread collects and sends what the threads have buffered, so a line
    //                     never waits longer than this for its thread to fill a buffer. Must be positive and at
    //                     most BufferingDefaults::MaxFlushInterval (30 seconds) when buffering is enabled;
    //                     std::runtime_error otherwise.
    //   fullBufferPolicy: what a thread does when it has no buffer space left (see FullBufferPolicy).
    WriterConfig(const std::string& type, unsigned int bufferSize,
                 std::chrono::milliseconds flushInterval = BufferingDefaults::FlushInterval,
                 FullBufferPolicy fullBufferPolicy = BufferingDefaults::FullBuffer);

    [[nodiscard]] const WriterType& GetType() const noexcept { return m_type; }
    [[nodiscard]] unsigned int GetBufferSize() const noexcept { return m_bufferSize; }
    [[nodiscard]] std::chrono::milliseconds GetFlushInterval() const noexcept { return m_flushInterval; }
    [[nodiscard]] FullBufferPolicy GetFullBufferPolicy() const noexcept { return m_fullBufferPolicy; }
    [[nodiscard]] const std::string& GetLocation() const noexcept { return m_location; }

   private:
    WriterType m_type;
    std::string m_location;
    unsigned int m_bufferSize = 0;
    std::chrono::milliseconds m_flushInterval = BufferingDefaults::FlushInterval;
    FullBufferPolicy m_fullBufferPolicy = BufferingDefaults::FullBuffer;
};

}  // namespace spectator
