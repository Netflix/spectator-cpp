#pragma once

#include <meter.h>
#include <meter_id.h>
#include <writer.h>

#include <string>

namespace spectator {

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

}  // namespace spectator
