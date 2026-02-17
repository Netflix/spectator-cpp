#include "lock_free_buffered_write_mode.h"
#include <logger.h>

namespace spectator {

static constexpr auto NEW_LINE = '\n';

LockFreeBufferedWriteMode::LockFreeBufferedWriteMode(BaseWriter& writer, size_t queueCapacity, size_t batchSize,
                                                     std::chrono::seconds flushInterval)
    : m_writer(writer), m_queue(queueCapacity), m_batchSize(batchSize), m_flushInterval(flushInterval)
{
    m_consumerThread = std::thread(&LockFreeBufferedWriteMode::ConsumerThread, this);
}

LockFreeBufferedWriteMode::~LockFreeBufferedWriteMode()
{
    m_shutdown.store(true);
    if (m_consumerThread.joinable())
    {
        m_consumerThread.join();
    }
}

void LockFreeBufferedWriteMode::Write(const std::string& message)
{
    if (!m_queue.TryPush(message))
    {
        Logger::warn("Lock-free write queue full, dropping message");
    }
}

void LockFreeBufferedWriteMode::ConsumerThread()
{
    std::string buffer;
    buffer.reserve(m_batchSize);
    std::string message;
    auto lastFlush = std::chrono::steady_clock::now();

    while (!m_shutdown.load())
    {
        bool dequeued = false;

        // Drain available messages from the queue
        while (m_queue.TryPop(message))
        {
            dequeued = true;
            buffer.append(message);
            buffer.push_back(NEW_LINE);

            if (buffer.size() >= m_batchSize)
            {
                m_writer.Write(buffer);
                buffer.clear();
                lastFlush = std::chrono::steady_clock::now();
            }
        }

        // Time-based flush
        auto now = std::chrono::steady_clock::now();
        if (!buffer.empty() && (now - lastFlush) >= m_flushInterval)
        {
            m_writer.Write(buffer);
            buffer.clear();
            lastFlush = now;
        }

        // Avoid burning CPU when queue is empty
        if (!dequeued)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Drain remaining messages on shutdown
    while (m_queue.TryPop(message))
    {
        buffer.append(message);
        buffer.push_back(NEW_LINE);
    }
    if (!buffer.empty())
    {
        m_writer.Write(buffer);
    }
}

}  // namespace spectator
