#pragma once

#include <meter.h>
#include <meter_id.h>

#include <string>

namespace spectator {

static constexpr auto COUNTER_TYPE_SYMBOL = "c";

class Counter final : public Meter
{
   public:
    explicit Counter(MeterId meter_id) : Meter(std::move(meter_id), COUNTER_TYPE_SYMBOL) {}

    void Increment(const double& delta = 1) const
    {
        if (delta > 0)
        {
            this->ConstructLine(delta);
        }
    }
};

}  // namespace spectator
