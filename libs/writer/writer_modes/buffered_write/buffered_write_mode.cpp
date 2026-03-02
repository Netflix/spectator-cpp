#include "buffered_write_mode.h"
#include <logger.h>

namespace spectator {

static constexpr auto NEW_LINE = '\n';

BufferedWriteMode::BufferedWriteMode(std::unique_ptr<BaseWriter> writer, unsigned int bufferSize)
    : WriteMode(std::move(writer)), m_bufferSize(bufferSize)
{
    m_buffer.reserve(m_bufferSize);
    m_sendingThread = std::thread(&BufferedWriteMode::ThreadSend, this);
}

BufferedWriteMode::~BufferedWriteMode()
{
    m_shutdown.store(true);
    m_cv_receiver.notify_all();
    m_cv_sender.notify_all();
    if (m_sendingThread.joinable())
    {
        m_sendingThread.join();
    }
}

void BufferedWriteMode::Write(const std::string& message)
{
    bool bufferFull = false;
    {
        std::unique_lock<std::mutex> lock(m_writeMutex);
        m_cv_receiver.wait(lock, [this] { return m_buffer.size() < m_bufferSize || m_shutdown.load(); });
        if (m_shutdown.load())
        {
            Logger::info("Write operation aborted due to shutdown signal");
            return;
        }
        m_buffer.append(message);
        m_buffer.push_back(NEW_LINE);
        bufferFull = m_buffer.size() >= m_bufferSize;
    }
    bufferFull ? m_cv_sender.notify_one() : m_cv_receiver.notify_one();
}

void BufferedWriteMode::ThreadSend()
{
    std::string message{};
    message.reserve(m_bufferSize);
    while (m_shutdown.load() == false)
    {
        {
            std::unique_lock<std::mutex> lock(m_writeMutex);
            m_cv_sender.wait(lock, [this] { return m_buffer.size() >= m_bufferSize || m_shutdown.load(); });
            if (m_shutdown.load() == true)
            {
                return;
            }
            message.swap(m_buffer);
            m_buffer.clear();
        }
        m_cv_receiver.notify_one();
        m_writer->Send(message);
    }
}

}  // namespace spectator
