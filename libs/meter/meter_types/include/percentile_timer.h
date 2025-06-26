#pragma once

#include <libs/meter/meter_types/include/meter.h>
#include <libs/meter/meter_id/meter_id.h>
#include <libs/writer/writer_wrapper/writer.h>

#include <string>

static constexpr auto PERCENTILE_TIMER_TYPE_SYMBOL = "T";

class PercentileTimer final : public Meter
{
   public:
    explicit PercentileTimer(const MeterId& meter_id) : Meter(meter_id, PERCENTILE_TIMER_TYPE_SYMBOL) {}

    void Record(const double& seconds) const
    {
        if (seconds >= 0)
        {
            auto line = this->ConstructLine(seconds);
            Writer::GetInstance().Write(line);
        }
    }
};
