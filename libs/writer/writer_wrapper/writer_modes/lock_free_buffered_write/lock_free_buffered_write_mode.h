#pragma once

#include <write_mode.h>
#include <base_writer.h>
#include "mpsc_queue.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace spectator {

class LockFreeBufferedWriteMode final : public WriteMode
{
   public:
    LockFreeBufferedWriteMode(BaseWriter& writer, size_t queueCapacity, size_t batchSize,
                              std::chrono::seconds flushInterval = std::chrono::seconds(10));
    ~LockFreeBufferedWriteMode() override;

    void Write(const std::string& message) override;

   private:
    void ConsumerThread();

    BaseWriter& m_writer;
    MPSCQueue<std::string> m_queue;
    size_t m_batchSize;
    std::chrono::seconds m_flushInterval;

    std::thread m_consumerThread;
    std::atomic<bool> m_shutdown{false};
};

}  // namespace spectator
