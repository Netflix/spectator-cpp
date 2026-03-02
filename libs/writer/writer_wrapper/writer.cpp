#include <writer.h>

#include <buffered_write_mode.h>
#include <non_buffered_write_mode.h>
#include <thread_local_buffered_write_mode.h>
#include <logger.h>
#include <stdexcept>

namespace spectator {

Writer::~Writer()
{
    auto& instance = GetInstance();

    // Destroying write mode stops any buffered send thread, then closes the underlying writer
    instance.m_writeMode.reset();
}

void Writer::Initialize(WriterType type, const std::string& param, int port, unsigned int bufferSize,
                        WriteModeType modeType)
{
    auto& instance = GetInstance();

    try
    {
        switch (modeType)
        {
            case WriteModeType::NonBuffered:
                instance.m_writeMode = std::make_unique<NonBufferedWriteMode>(type, param, port);
                break;
            case WriteModeType::Buffered:
                instance.m_writeMode = std::make_unique<BufferedWriteMode>(type, bufferSize, param, port);
                break;
            case WriteModeType::ThreadLocalBuffered:
                instance.m_writeMode = std::make_unique<ThreadLocalBufferedWriteMode>(type, bufferSize,
                                                                                       std::chrono::seconds(10),
                                                                                       param, port);
                break;
            default:
                throw std::runtime_error("Unsupported write mode type");
        }
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to initialize writer: {}", e.what());
        throw;
    }
}

void Writer::Write(const std::string& message)
{
    auto& instance = GetInstance();

    if (!instance.m_writeMode)
    {
        Logger::error("Attempted to write with uninitialized writer");
        return;
    }

    instance.m_writeMode->Write(message);
}

void Writer::Close()
{
    const auto& instance = GetInstance();

    if (!instance.m_writeMode)
    {
        Logger::error("Close called on uninitialized writer");
        return;
    }

    try
    {
        instance.m_writeMode->GetWriter()->Close();
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to close writer: {}", e.what());
    }
}

}  // namespace spectator
