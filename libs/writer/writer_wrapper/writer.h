#pragma once

#include <singleton.h>
#include <writer_types.h>
#include <write_mode.h>

#include <memory>
#include <string>

namespace spectator {

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
    Writer() = default;

    static void Initialize(WriterType type, const std::string& param = "", int port = 0, unsigned int bufferSize = 0,
                           WriteModeType modeType = WriteModeType::Auto);

    static void Write(const std::string& message);

    void Close();

    // Get the Writer's implementation for testing purposes
    static BaseWriter* GetImpl() { return Writer::GetInstance().m_impl.get(); }
    static WriterType GetWriterType() { return GetInstance().m_currentType; }

    std::unique_ptr<BaseWriter> m_impl;
    WriterType m_currentType = WriterType::Memory;  // Default type

    // m_writeMode must be declared after m_impl so it is destroyed first
    std::unique_ptr<WriteMode> m_writeMode;
};

}  // namespace spectator
