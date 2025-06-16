#pragma once

#include <libs/utils/include/singleton.h>
#include <libs/writer/writer_types/include/writer_types.h>

#include <memory>
#include <string>

class Writer final : public Singleton<Writer>
{
   public:
    virtual ~Writer();

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

    static void Initialize(WriterType type, const std::string& param = "", int port = 0, unsigned int bufferSize = 0);

    static void Write(const std::string& message);

    void BufferedWrite(const std::string& message);

    void NonBufferedWrite(const std::string& message);

    void ThreadSend();

    void TryToSend(const std::string& message);

    void Close();

    // Get the Writer's implementation for testing purposes
    static BaseWriter* GetImpl() { return Writer::GetInstance().m_impl.get(); }
    static WriterType GetWriterType() { return GetInstance().m_currentType; }


    static void Reset();

    std::unique_ptr<BaseWriter> m_impl;
    WriterType m_currentType = WriterType::Memory;  // Default type
    bool bufferingEnabled = false;
    unsigned int bufferSize = 0;
    std::string buffer{};
    
    // Function pointer for write strategy - member function pointer
    using WriteFunction = void (Writer::*)(const std::string&);
    WriteFunction writeImpl = &Writer::NonBufferedWrite;  // Default to non-buffered


    // multithreading writes
    std::mutex writeMutex;
    std::thread sendingThread;
    std::condition_variable cv_receiver;
    std::condition_variable cv_sender;
    std::atomic<bool> shutdown{false};
};