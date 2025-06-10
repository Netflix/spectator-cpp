#include <libs/writer/writer_wrapper/include/Writer.h>

#include <libs/writer/writer_types/include/writer_types.h>
#include <libs/logger/logger.h>
#include <stdexcept>

Writer::~Writer()
{
    // No need to explicitly close here as unique_ptr will clean up
}

void Writer::Initialize(WriterType type, const std::string &param, int port)
{
    // Get the singleton instance directly
    auto &instance = GetInstance();

    // Create the new writer based on type
    try
    {
        switch (type)
        {
        case WriterType::Memory:
            instance.m_impl = std::make_unique<MemoryWriter>();
            Logger::info("Writer initialized as MemoryWriter");
            break;
        case WriterType::UDP:
            instance.m_impl = std::make_unique<UDPWriter>(param, port);
            Logger::info("Writer initialized as UDPWriter with host: {} and port: {}", param, port);
            break;
        case WriterType::Unix:
            instance.m_impl = std::make_unique<UDSWriter>(param);
            Logger::info("Writer initialized as UnixWriter with socket path: {}", param);
            break;
        default:
            throw std::runtime_error("Unsupported writer type");
        }

        instance.m_currentType = type;
    }
    catch (const std::exception &e)
    {
        Logger::error("Failed to initialize writer: {}", e.what());
        throw;
    }
}

void Writer::Reset()
{
    auto &instance = GetInstance();

    if (instance.m_impl)
    {
        try
        {
            instance.m_impl->Close();
        }
        catch (const std::exception &e)
        {
            Logger::warn("Exception while closing writer during reset: {}", e.what());
        }
    }

    instance.m_impl.reset();
    Logger::info("Writer has been reset");
}

void Writer::Write(const std::string &message)
{
    auto &instance = GetInstance();

    if (!instance.m_impl)
    {
        Logger::error("Attempted to write with uninitialized writer implementation");
        return;
    }

    try
    {
        instance.m_impl->Write(message);
    }
    catch (const std::exception &e)
    {
        Logger::error("Write operation failed: {}", e.what());
    }
}

void Writer::Close()
{
    auto &instance = GetInstance();

    if (!instance.m_impl)
    {
        Logger::debug("Close called on uninitialized writer");
        return;
    }

    try
    {
        instance.m_impl->Close();
        Logger::debug("Writer closed successfully");
    }
    catch (const std::exception &e)
    {
        Logger::error("Failed to close writer: {}", e.what());
    }
}

WriterType Writer::GetWriterType()
{
    return GetInstance().m_currentType;
}
