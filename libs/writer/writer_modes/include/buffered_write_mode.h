#pragma once

#include <write_mode.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace spectator {

class BufferedWriteMode final : public WriteMode
{
   public:
    BufferedWriteMode(WriterType type, unsigned int bufferSize, const std::string& param = "", int port = 0);
    ~BufferedWriteMode() override;

    WriteModeType GetType() const override { return WriteModeType::Buffered; }
    void Write(const std::string& message) override;

   private:
    void ThreadSend();

    unsigned int m_bufferSize;
    std::string m_buffer;

    std::mutex m_writeMutex;
    std::thread m_sendingThread;
    std::condition_variable m_cv_receiver;
    std::condition_variable m_cv_sender;
    std::atomic<bool> m_shutdown{false};
};

}  // namespace spectator
