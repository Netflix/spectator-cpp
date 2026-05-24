#pragma once

#include <meter.h>
#include <meter_id.h>

#include <string>

namespace spectator {

static constexpr auto TIMER_TYPE_SYMBOL = "t";

class Timer final : public Meter
{
   public:
    explicit Timer(MeterId meter_id) : Meter(std::move(meter_id), TIMER_TYPE_SYMBOL) {}

    void Record(const double& seconds) const
    {
        if (seconds >= 0)
        {
            this->ConstructLine(seconds);
        }
    }
};

}  // namespace spectator