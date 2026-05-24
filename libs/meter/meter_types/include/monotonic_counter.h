#pragma once

#include <meter.h>
#include <meter_id.h>

#include <string>

namespace spectator {

static constexpr auto MONOTONIC_COUNTER_TYPE_SYMBOL = "C";

class MonotonicCounter final : public Meter
{
   public:
    explicit MonotonicCounter(MeterId meter_id) : Meter(std::move(meter_id), MONOTONIC_COUNTER_TYPE_SYMBOL) {}

    void Set(const double& amount) const
    {
        this->ConstructLine(amount);
    }
};

}  // namespace spectator
