#pragma once

#include <writer.h>

#include <chrono>
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
                                 unsigned int bufferSize = 0,
                                 std::chrono::milliseconds flushInterval = BufferingDefaults::FlushInterval,
                                 FullBufferPolicy fullBufferPolicy = BufferingDefaults::FullBuffer)
    {
        Writer::Initialize(type, param, port, bufferSize, flushInterval, fullBufferPolicy);
    }

    // Get the Writer's implementation for testing purposes
    static BaseWriter* GetImpl() { return Writer::GetInstance().m_impl.get(); }
};

}  // namespace spectator
