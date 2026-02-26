#include "thread_local_buffered_write_mode.h"
#include <logger.h>

namespace spectator {

static constexpr auto NEW_LINE = '\n';

ThreadLocalBufferedWriteMode::ThreadLocalBufferedWriteMode(BaseWriter& writer, size_t bufferSize,
                                                             std::chrono::seconds flushInterval)
    : m_writer(writer), m_bufferSize(bufferSize), m_flushInterval(flushInterval)
{
    m_flushThread = std::thread(&ThreadLocalBufferedWriteMode::FlushThread, this);
}

ThreadLocalBufferedWriteMode::~ThreadLocalBufferedWriteMode()
{
    m_shutdown.store(true);
    if (m_flushThread.joinable())
    {
        m_flushThread.join();
    }

    // Flush all remaining registered buffers
    std::lock_guard<std::mutex> rlock(m_registryMutex);
    for (auto* tb : m_buffers)
    {
        std::lock_guard<std::mutex> lock(tb->mutex);
        if (!tb->buffer.empty())
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
        tld.buffer.buffer.reserve(m_bufferSize);
        RegisterBuffer(&tld.buffer);
    }

    auto& tb = tld.buffer;
    std::lock_guard<std::mutex> lock(tb.mutex);
    tb.buffer.append(message);
    tb.buffer.push_back(NEW_LINE);

    if (tb.buffer.size() >= m_bufferSize)
    {
        FlushBuffer(tb);
    }
}

void ThreadLocalBufferedWriteMode::FlushBuffer(ThreadBuffer& tb)
{
    std::lock_guard<std::mutex> wlock(m_writerMutex);
    m_writer.Write(tb.buffer);
    tb.buffer.clear();
    tb.lastFlush = std::chrono::steady_clock::now();
}

void ThreadLocalBufferedWriteMode::FlushThread()
{
    while (!m_shutdown.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::lock_guard<std::mutex> rlock(m_registryMutex);
        auto now = std::chrono::steady_clock::now();
        for (auto* tb : m_buffers)
        {
            std::lock_guard<std::mutex> lock(tb->mutex);
            if (!tb->buffer.empty() && (now - tb->lastFlush) >= m_flushInterval)
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
            if (!buffer.buffer.empty())
            {
                owner->FlushBuffer(buffer);
            }
        }
        owner->DeregisterBuffer(&buffer);
    }
}

}  // namespace spectator
