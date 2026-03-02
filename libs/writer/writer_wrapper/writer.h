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
                           WriteModeType modeType = WriteModeType::NonBuffered);

    static void Write(const std::string& message);

    void Close();

    static WriterType GetWriterType() { return GetInstance().m_writeMode->GetWriter()->GetType(); }
    static WriteModeType GetWriteModeType() { return GetInstance().m_writeMode->GetType(); }

    // m_writeMode owns both the write strategy and the underlying BaseWriter
    std::unique_ptr<WriteMode> m_writeMode;
};

}  // namespace spectator
