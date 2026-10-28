#pragma once

#include <writer.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace spectator {

/**
 * WriterTestHelper - A utility class to help with testing Writer functionality
 *
 * This class is a friend of Writer and can provide access to Writer's private
 * methods for testing purposes.
 */
class WriterTestHelper
{
   public:
    // Initialize the Writer for testing purposes
    static void InitializeWriter(WriterType type, const std::string& param = "", int port = 0,
                                 unsigned int bufferSize = 0)
    {
        Writer::Initialize(type, param, port, BufferConfig{.size = bufferSize});
    }

    static void InitializeWriter(WriterType type, const std::string& param, int port, const BufferConfig& buffer)
    {
        Writer::Initialize(type, param, port, buffer);
    }

    // Installs any transport, buffered whenever buffer.size > 0, including a MemoryWriter or a test transport.
    static void InitializeWithTransport(std::unique_ptr<BaseWriter> transport, const BufferConfig& buffer = {})
    {
        Writer::InitializeWithTransport(std::move(transport), buffer);
    }

    // Get the Writer's implementation for testing purposes
    static BaseWriter* GetImpl() { return Writer::GetImpl(); }

    // Sends everything pending and closes the current configuration.
    static void Shutdown() { Writer::Shutdown(); }

    // Runs one drain pass now, on the calling thread.
    static void Flush() { Writer::Flush(); }

    static std::uint64_t DroppedCount() { return Writer::DroppedCount(); }

    static std::size_t RegisteredBufferCount() { return Writer::RegisteredBufferCount(); }
};

}  // namespace spectator
