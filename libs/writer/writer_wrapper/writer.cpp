#include <writer.h>

#include <writer_types.h>
#include <logger.h>
#include <stdexcept>

namespace spectator {

static constexpr auto NEW_LINE = '\n';

// Each worker thread owns a shared_ptr to its buffer.
// The registry in Writer holds weak_ptrs so threads can exit freely.
thread_local std::shared_ptr<Writer::ThreadLocalBuffer> tl_buffer;

// Flush any data remaining in the buffer when the thread exits.
Writer::ThreadLocalBuffer::~ThreadLocalBuffer()
{
    if (data.empty())
    {
        return;
    }
    try
    {
        auto& instance = Writer::GetInstance();
        if (instance.m_impl)
        {
            instance.TryToSend(data);
        }
    }
    catch (...)
    {
        // Best-effort: ignore errors during thread teardown
    }
}

Writer::~Writer()
{
    auto& instance = GetInstance();

    // Signal all background threads to stop
    {
        std::lock_guard<std::mutex> lock(instance.shutdownMutex);
        instance.shutdown.store(true);
    }
    instance.cv_shutdown.notify_all();

    if (instance.flushTimerThread.joinable())
    {
        instance.flushTimerThread.join();
    }

    this->Close();
}

void Writer::Initialize(WriterType type, const std::string& param, int port,
                        unsigned int bufferSize, unsigned int flushIntervalMs)
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

        // Stop any pre-existing timer thread before re-initialising
        {
            std::lock_guard<std::mutex> lock(instance.shutdownMutex);
            instance.shutdown.store(true);
        }
        instance.cv_shutdown.notify_all();
        if (instance.flushTimerThread.joinable())
        {
            instance.flushTimerThread.join();
        }
        // Clear the thread-local buffer registry from any previous init
        {
            std::lock_guard<std::mutex> regLock(instance.registryMutex);
            instance.threadBufferRegistry.clear();
        }
        instance.shutdown.store(false);
        instance.bufferingEnabled = false;

        if (bufferSize > 0 || flushIntervalMs > 0)
        {
            instance.bufferingEnabled = true;
            instance.bufferSize = bufferSize;
            instance.writeImpl = &Writer::ThreadLocalBufferedWrite;

            if (flushIntervalMs > 0)
            {
                instance.flushInterval = std::chrono::milliseconds(flushIntervalMs);
                instance.flushTimerThread = std::thread(&Writer::FlushTimerThread, &instance);
                Logger::info("Writer interval flush enabled: {}ms", flushIntervalMs);
            }
        }
        else
        {
            instance.writeImpl = &Writer::NonBufferedWrite;
        }
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to initialize writer: {}", e.what());
        throw;
    }
}

void Writer::TryToSend(const std::string& message)
{
    const auto& instance = GetInstance();
    instance.m_impl->Write(message);
}

void Writer::FlushTimerThread()
{
    auto& instance = GetInstance();

    while (true)
    {
        // Sleep for the flush interval, or wake early on shutdown
        {
            std::unique_lock<std::mutex> lock(instance.shutdownMutex);
            instance.cv_shutdown.wait_for(lock, instance.flushInterval,
                [&instance] { return instance.shutdown.load(); });
        }

        if (instance.shutdown.load())
        {
            break;
        }

        // Collect live buffers (clean up expired weak_ptrs in the same pass)
        std::vector<std::shared_ptr<ThreadLocalBuffer>> buffersToFlush;
        {
            std::lock_guard<std::mutex> regLock(instance.registryMutex);
            for (auto it = instance.threadBufferRegistry.begin();
                 it != instance.threadBufferRegistry.end();)
            {
                if (auto buf = it->lock())
                {
                    buffersToFlush.push_back(std::move(buf));
                    ++it;
                }
                else
                {
                    it = instance.threadBufferRegistry.erase(it);
                }
            }
        }

        // Drain each thread-local buffer
        for (auto& buf : buffersToFlush)
        {
            std::string toSend;
            {
                std::lock_guard<std::mutex> lock(buf->mutex);
                if (!buf->data.empty())
                {
                    toSend = std::move(buf->data);
                    buf->data.clear();
                }
            }
            if (!toSend.empty())
            {
                instance.TryToSend(toSend);
            }
        }
    }
}

void Writer::ThreadLocalBufferedWrite(const std::string& message)
{
    auto& instance = GetInstance();

    // Initialise this thread's buffer on first use and register it
    if (!tl_buffer)
    {
        tl_buffer = std::make_shared<ThreadLocalBuffer>();
        std::lock_guard<std::mutex> regLock(instance.registryMutex);
        instance.threadBufferRegistry.push_back(tl_buffer);
    }

    std::string toSend;
    {
        std::lock_guard<std::mutex> lock(tl_buffer->mutex);
        tl_buffer->data.append(message);
        tl_buffer->data.push_back(NEW_LINE);

        // Capacity-based flush: drain when the thread-local buffer is full
        if (instance.bufferSize > 0 && tl_buffer->data.size() >= instance.bufferSize)
        {
            toSend = std::move(tl_buffer->data);
            tl_buffer->data.clear();
        }
    }

    if (!toSend.empty())
    {
        instance.TryToSend(toSend);
    }
}

void Writer::NonBufferedWrite(const std::string& message)
{
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

    (instance.*instance.writeImpl)(message);
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
