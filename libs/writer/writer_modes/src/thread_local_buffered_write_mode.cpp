#include "thread_local_buffered_write_mode.h"
#include <logger.h>

namespace spectator {

static constexpr auto NEW_LINE = '\n';

ThreadLocalBufferedWriteMode::ThreadLocalBufferedWriteMode(WriterType type, size_t bufferSize,
                                                             std::chrono::seconds flushInterval,
                                                             const std::string& param, int port)
    : WriteMode(type, param, port), m_bufferSize(bufferSize), m_flushInterval(flushInterval)
{
    Logger::info("WriteMode mode: ThreadLocalBuffered, buffer size: {}, flush interval: {}s",
                 m_bufferSize, m_flushInterval.count());
    m_flushThread = std::thread(&ThreadLocalBufferedWriteMode::FlushThread, this);
}

ThreadLocalBufferedWriteMode::~ThreadLocalBufferedWriteMode()
{
    m_shutdown.store(true);
    m_shutdownCv.notify_one();
    if (m_flushThread.joinable())
    {
        m_flushThread.join();
    }

    // Flush all remaining registered buffers
    std::lock_guard<std::mutex> rlock(m_registryMutex);
    for (auto* tb : m_buffers)
    {
        std::lock_guard<std::mutex> lock(tb->mutex);
        if (!tb->data.empty())
        {
            FlushBuffer(*tb);
        }
    }
}

void ThreadLocalBufferedWriteMode::Write(const std::string& message)
{
    thread_local ThreadLocalData tld;
    if (tld.owner == nullptr)
    {
        tld.owner = this;
        tld.buffer.data.reserve(m_bufferSize);
        RegisterBuffer(&tld.buffer);
    }

    auto& tb = tld.buffer;
    std::lock_guard<std::mutex> lock(tb.mutex);
    tb.data.append(message);
    tb.data.push_back(NEW_LINE);

    if (tb.data.size() >= m_bufferSize)
    {
        FlushBuffer(tb);
    }
}

void ThreadLocalBufferedWriteMode::FlushBuffer(ThreadBuffer& tb)
{
    std::lock_guard<std::mutex> wlock(m_writerMutex);
    m_writer->Send(tb.data);
    tb.data.clear();
    tb.lastFlush = std::chrono::steady_clock::now();
}

void ThreadLocalBufferedWriteMode::FlushThread()
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lk(m_shutdownMutex);
            if (m_shutdownCv.wait_for(lk, m_flushInterval, [this] { return m_shutdown.load(); }))
            {
                return;
            }
        }

        std::lock_guard<std::mutex> rlock(m_registryMutex);
        auto now = std::chrono::steady_clock::now();
        for (auto* tb : m_buffers)
        {
            std::lock_guard<std::mutex> lock(tb->mutex);
            if (!tb->data.empty() && (now - tb->lastFlush) >= m_flushInterval)
            {
                FlushBuffer(*tb);
            }
        }
    }
}

void ThreadLocalBufferedWriteMode::RegisterBuffer(ThreadBuffer* tb)
{
    std::lock_guard<std::mutex> lock(m_registryMutex);
    m_buffers.push_back(tb);
}

void ThreadLocalBufferedWriteMode::DeregisterBuffer(ThreadBuffer* tb)
{
    std::lock_guard<std::mutex> lock(m_registryMutex);
    m_buffers.erase(std::remove(m_buffers.begin(), m_buffers.end(), tb), m_buffers.end());
}

ThreadLocalBufferedWriteMode::ThreadLocalData::~ThreadLocalData()
{
    if (owner != nullptr && !owner->m_shutdown.load())
    {
        {
            std::lock_guard<std::mutex> lock(buffer.mutex);
            if (!buffer.data.empty())
            {
                owner->FlushBuffer(buffer);
            }
        }
        owner->DeregisterBuffer(&buffer);
    }
}

}  // namespace spectator
