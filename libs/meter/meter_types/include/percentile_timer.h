#pragma once

#include <meter.h>
#include <meter_id.h>

#include <string>

namespace spectator {

static constexpr auto PERCENTILE_TIMER_TYPE_SYMBOL = "T";

class PercentileTimer final : public Meter
{
   public:
    explicit PercentileTimer(MeterId meter_id) : Meter(std::move(meter_id), PERCENTILE_TIMER_TYPE_SYMBOL) {}

    void Record(const double& seconds) const
    {
        if (seconds >= 0)
        {
            this->ConstructLine(seconds);
        }
    }
};

}  // namespace spectator
