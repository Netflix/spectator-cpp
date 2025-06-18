#include <libs/writer/writer_wrapper/writer.h>

#include <libs/writer/writer_types/include/writer_types.h>
#include <libs/logger/logger.h>
#include <stdexcept>

static constexpr auto NEW_LINE = '\n';

Writer::~Writer()
{
    auto& instance = GetInstance();

    if (instance.bufferingEnabled)
    {
        instance.shutdown.store(true);
        instance.cv_receiver.notify_all();
        instance.cv_sender.notify_all();
        if (instance.sendingThread.joinable()) {
            instance.sendingThread.join();
        }
    }
}

void Writer::Initialize(WriterType type, const std::string& param, int port, unsigned int bufferSize)
{
    // Get the singleton instance directly
    auto& instance = GetInstance();

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
        
        if (bufferSize > 0)
        {
            instance.bufferingEnabled = true;
            instance.bufferSize = bufferSize;
            instance.buffer.reserve(bufferSize);
            instance.writeImpl = &Writer::BufferedWrite;
            // Create a thread with proper binding to the instance method
            instance.sendingThread = std::thread(&Writer::ThreadSend, &instance);
        }
        else
        {
            // Explicitly set to non-buffered if buffer size is 0
            instance.writeImpl = &Writer::NonBufferedWrite;
        }
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to initialize writer: {}", e.what());
        throw;
    }
}

void Writer::Reset()
{
    auto& instance = GetInstance();

    if (instance.m_impl)
    {
        try
        {
            instance.m_impl->Close();
        }
        catch (const std::exception& e)
        {
            Logger::warn("Exception while closing writer during reset: {}", e.what());
        }
    }

    instance.m_impl.reset();
    Logger::info("Writer has been reset");
}


void Writer::TryToSend(const std::string& message)
{
    const auto& instance = GetInstance();

    if (!instance.m_impl)
    {
        Logger::error("Attempted to send with uninitialized writer implementation");
        return;
    }

    try
    {
        instance.m_impl->Write(message);
        Logger::debug("Message sent successfully: {}", message);
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to send message: {}", e.what());
    }
}

void Writer::ThreadSend()
{
    auto& instance = GetInstance();
    std::string message{};
    while (instance.shutdown.load() == false)
    {
        {
            std::unique_lock<std::mutex> lock(instance.writeMutex);
            instance.cv_sender.wait(
                lock, [&instance] { return instance.buffer.size() > instance.bufferSize || instance.shutdown.load(); });
            if (instance.shutdown.load() == true)
            {
                return;
            }
            message = std::move(instance.buffer);
            instance.buffer = std::string();
            instance.buffer.reserve(instance.bufferSize);
        }
        instance.cv_receiver.notify_one();
        instance.TryToSend(message);
    }
}

void Writer::BufferedWrite(const std::string& message)
{
    auto& instance = GetInstance();

    if (!instance.m_impl)
    {
        Logger::error("Attempted to write with uninitialized writer implementation");
        return;
    }

    {
        std::unique_lock<std::mutex> lock(instance.writeMutex);
        // TODO: Optimize memory alloc to not exceed allocated size
        instance.cv_receiver.wait(
            lock, [&instance] { return instance.buffer.size() < instance.bufferSize || instance.shutdown.load(); });
        if (instance.shutdown.load())
        {
            Logger::warn("Write operation aborted due to shutdown signal");
            return;
        }
        instance.buffer.append(message);
        instance.buffer.push_back(NEW_LINE);
    }
    instance.buffer.size() > instance.bufferSize ? instance.cv_sender.notify_one() : instance.cv_receiver.notify_one();
}

void Writer::NonBufferedWrite(const std::string& message)
{
    // Since this is a non-static method, we're already operating on an instance
    // and can call the instance method directly
    this->TryToSend(message + NEW_LINE);
}

void Writer::Write(const std::string& message)
{
    auto& instance = GetInstance();

    if (!instance.m_impl)
    {
        Logger::error("Attempted to write with uninitialized writer implementation");
        return;
    }

    try
    {
        // Call the member function using the pointer-to-member syntax
        (instance.*instance.writeImpl)(message);
        Logger::debug("Message written successfully: {}", message);
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to write message: {}", e.what());
    }   
}

void Writer::Close()
{
    const auto& instance = GetInstance();

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
    catch (const std::exception& e)
    {
        Logger::error("Failed to close writer: {}", e.what());
    }
}