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

    static void Initialize(WriterType type, const std::string& param = "", int port = 0);

    static void Write(const std::string& message);

    static void Close();

    static WriterType GetWriterType();

    // Get the Writer's implementation for testing purposes
    static BaseWriter* GetImpl() { return Writer::GetInstance().m_impl.get(); }
    /**
     * Reset the writer to uninitialized state
     * This method is private and can only be called by Registry
     */
    static void Reset();

    // Implementation details
    std::unique_ptr<BaseWriter> m_impl;
    WriterType m_currentType = WriterType::Memory;  // Default type
};