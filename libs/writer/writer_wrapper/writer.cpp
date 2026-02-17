#include <writer.h>

#include <buffered_write_mode.h>
#include <non_buffered_write_mode.h>
#include <lock_free_buffered_write_mode.h>
#include <writer_types.h>
#include <logger.h>
#include <stdexcept>

namespace spectator {

Writer::~Writer()
{
    auto& instance = GetInstance();

    // Destroy write mode first to stop any buffered send thread
    instance.m_writeMode.reset();

    this->Close();
}

void Writer::Initialize(WriterType type, const std::string& param, int port, unsigned int bufferSize,
                        WriteModeType modeType)
{
    auto& instance = GetInstance();

    try
    {
        switch (type)
        {
            case WriterType::Memory:
                instance.m_impl = std::make_unique<MemoryWriter>();
                Logger::info("WriterWrapper initialized as MemoryWriter");
                break;
            case WriterType::UDP:
                instance.m_impl = std::make_unique<UDPWriter>(param, port);
                Logger::info("WriterWrapper initialized as UDPWriter with host: {} and port: {}", param, port);
                break;
            case WriterType::Unix:
                instance.m_impl = std::make_unique<UDSWriter>(param);
                Logger::info("WriterWrapper initialized as UnixWriter with socket path: {}", param);
                break;
            default:
                throw std::runtime_error("Unsupported writer type");
        }

        instance.m_currentType = type;

        // Auto: preserve original behavior based on bufferSize
        if (modeType == WriteModeType::Auto)
        {
            modeType = bufferSize > 0 ? WriteModeType::Buffered : WriteModeType::NonBuffered;
        }

        switch (modeType)
        {
            case WriteModeType::NonBuffered:
                instance.m_writeMode = std::make_unique<NonBufferedWriteMode>(*instance.m_impl);
                break;
            case WriteModeType::Buffered:
                instance.m_writeMode = std::make_unique<BufferedWriteMode>(*instance.m_impl, bufferSize);
                break;
            case WriteModeType::LockFreeBuffered:
                instance.m_writeMode = std::make_unique<LockFreeBufferedWriteMode>(*instance.m_impl, 8192, bufferSize);
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

    if (!instance.m_impl)
    {
        Logger::error("Attempted to write with uninitialized writer implementation");
        return;
    }

    instance.m_writeMode->Write(message);
}

void Writer::Close()
{
    const auto& instance = GetInstance();

    if (!instance.m_impl)
    {
        Logger::error("Close called on uninitialized writer");
        return;
    }

    try
    {
        instance.m_impl->Close();
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to close writer: {}", e.what());
    }
}

}  // namespace spectator
