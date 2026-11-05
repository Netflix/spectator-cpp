#pragma once

#include <singleton.h>
#include <writer_types.h>

#include <chrono>
#include <memory>
#include <string>

namespace spectator {

// Hands the lines that the meters record to the socket writer (UDPWriter, UDSWriter or MemoryWriter).
//
// Without buffering (buffer size 0), the calling thread sends each line immediately. This is only safe from one
// thread.
//
// With buffering, each thread appends its lines to its own buffer and one sender thread sends the buffers. Recording
// a line normally touches only the calling thread's own buffers. See buffered_writer.h and buffered_writer.cpp.
class Writer final : public Singleton<Writer>
{
   public:
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
    Writer();

    // Not thread-safe: call it while no other thread is recording. It may be called again to replace the writer; a
    // buffered writer from the earlier call is shut down first, which sends what it still has buffered.
    static void Initialize(WriterType type, const std::string& param = "", int port = 0, unsigned int bufferSize = 0,
                           std::chrono::milliseconds flushInterval = BufferingDefaults::FlushInterval,
                           FullBufferPolicy fullBufferPolicy = BufferingDefaults::FullBuffer);

    // Records one protocol line, given without its trailing newline. Thread-safe when buffering is enabled.
    static void Write(const std::string& line);

    void NonBufferedWrite(const std::string& message);

    void TryToSend(const std::string& message);

    void Close();

    // Get the Writer's implementation for testing purposes
    static BaseWriter* GetImpl() { return Writer::GetInstance().m_impl.get(); }
    static WriterType GetWriterType() { return GetInstance().m_currentType; }

    // The socket writer. With buffering enabled, the buffered writer does all the sending through it.
    std::unique_ptr<BaseWriter> m_impl;
    WriterType m_currentType = WriterType::Memory;  // Default type
};

}  // namespace spectator
