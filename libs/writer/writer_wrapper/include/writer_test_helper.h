#pragma once

#include <libs/writer/writer_wrapper/include/Writer.h>

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
    static void InitializeWriter(WriterType type, const std::string& param = "", int port = 0)
    {
        Writer::Initialize(type, param, port);
    }

    // Reset the Writer for testing purposes
    static void ResetWriter() { Writer::Reset(); }

    // Get the Writer's implementation for testing purposes
    static BaseWriter* GetImpl() { return Writer::GetInstance().m_impl.get(); }
};
