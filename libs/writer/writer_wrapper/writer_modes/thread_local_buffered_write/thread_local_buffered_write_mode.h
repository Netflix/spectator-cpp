#pragma once

#include <write_mode.h>
#include <base_writer.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace spectator {

class ThreadLocalBufferedWriteMode final : public WriteMode
{
   public:
    ThreadLocalBufferedWriteMode(BaseWriter& writer, size_t bufferSize,
                                  std::chrono::seconds flushInterval = std::chrono::seconds(10));
    ~ThreadLocalBufferedWriteMode() override;

    void Write(const std::string& message) override;

   private:
    struct ThreadBuffer
    {
        std::string buffer;
        std::mutex mutex;
        std::chrono::steady_clock::time_point lastFlush{std::chrono::steady_clock::now()};
    };

    struct ThreadLocalData
    {
        ThreadBuffer buffer;
        ThreadLocalBufferedWriteMode* owner = nullptr;
        ~ThreadLocalData();
    };

    void RegisterBuffer(ThreadBuffer* tb);
    void DeregisterBuffer(ThreadBuffer* tb);
    void FlushBuffer(ThreadBuffer& tb);
    void FlushThread();

    BaseWriter& m_writer;
    size_t m_bufferSize;
    std::chrono::seconds m_flushInterval;

    std::mutex m_registryMutex;
    std::vector<ThreadBuffer*> m_buffers;

    std::mutex m_writerMutex;

    std::thread m_flushThread;
    std::atomic<bool> m_shutdown{false};
};

}  // namespace spectator
