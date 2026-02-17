#pragma once

#include <write_mode.h>
#include <base_writer.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace spectator {

class BufferedWriteMode final : public WriteMode
{
   public:
    BufferedWriteMode(BaseWriter& writer, unsigned int bufferSize);
    ~BufferedWriteMode() override;

    void Write(const std::string& message) override;

   private:
    void ThreadSend();

    BaseWriter& m_writer;
    unsigned int m_bufferSize;
    std::string m_buffer;

    std::mutex m_writeMutex;
    std::thread m_sendingThread;
    std::condition_variable m_cv_receiver;
    std::condition_variable m_cv_sender;
    std::atomic<bool> m_shutdown{false};
};

}  // namespace spectator
