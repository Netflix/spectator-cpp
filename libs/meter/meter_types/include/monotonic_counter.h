#pragma once

#include <meter.h>
#include <meter_id.h>
#include <writer.h>

#include <string>

namespace spectator {

static constexpr auto MONOTONIC_COUNTER_TYPE_SYMBOL = "C";

class MonotonicCounter final : public Meter
{
   public:
    explicit MonotonicCounter(const MeterId& meter_id) : Meter(meter_id, MONOTONIC_COUNTER_TYPE_SYMBOL) {}

    void Set(const double& amount) const
    {
        auto line = this->ConstructLine(amount);
        Writer::GetInstance().Write(line);
    }
};

}  // namespace spectator
