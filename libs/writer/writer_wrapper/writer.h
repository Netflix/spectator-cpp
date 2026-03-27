#pragma once

#include <singleton.h>
#include <writer_types.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace spectator {

class Writer final : public Singleton<Writer>
{
   public:
    // Per-thread buffer: each worker thread gets its own; avoids global mutex on writes.
    struct ThreadLocalBuffer
    {
        std::string data;
        std::mutex mutex;
        ~ThreadLocalBuffer();  // Flushes any remaining data on thread exit
    };

    ~Writer() override;

   private:
    friend class Singleton<Writer>;
    friend class Registry;
    friend class WriterTestHelper;
    friend class AgeGauge;
    friend class Counter;
    friend class DistributionSummary;
    friend class Gauge;
    friend class MaxGauge;
    friend class MonotonicCounter;
    friend class MonotonicCounterUint;
    friend class PercentileDistributionSummary;
    friend class PercentileTimer;
    friend class Timer;

    // Private constructor - enforces singleton pattern
    Writer() = default;

    static void Initialize(WriterType type, const std::string& param = "", int port = 0,
                           unsigned int bufferSize = 0);

    static void Write(const std::string& message);

    void ThreadLocalBufferedWrite(const std::string& message);

    void NonBufferedWrite(const std::string& message);

    void TryToSend(const std::string& message);

    void Close();

    // Get the Writer's implementation for testing purposes
    static BaseWriter* GetImpl() { return Writer::GetInstance().m_impl.get(); }
    static WriterType GetWriterType() { return GetInstance().m_currentType; }

    std::unique_ptr<BaseWriter> m_impl;
    WriterType m_currentType = WriterType::Memory;
    bool bufferingEnabled = false;
    unsigned int bufferSize = 0;

    // Function pointer for write strategy
    using WriteFunction = void (Writer::*)(const std::string&);
    WriteFunction writeImpl = &Writer::NonBufferedWrite;

    // Registry of all active thread-local buffers (weak_ptrs to avoid keeping threads alive)
    std::mutex registryMutex;
    std::vector<std::weak_ptr<ThreadLocalBuffer>> threadBufferRegistry;

    std::atomic<bool> shutdown{false};
};

}  // namespace spectator
