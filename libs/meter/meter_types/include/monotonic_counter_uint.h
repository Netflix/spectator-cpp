#pragma once

#include <meter.h>
#include <meter_id.h>

#include <string>

namespace spectator {

static constexpr auto MONOTONIC_COUNTER_UINT_TYPE_SYMBOL = "U";

class MonotonicCounterUint final : public Meter
{
   public:
    explicit MonotonicCounterUint(MeterId meter_id) : Meter(std::move(meter_id), MONOTONIC_COUNTER_UINT_TYPE_SYMBOL) {}

    void Set(const uint64_t& amount) const
    {
        this->ConstructLine(amount);
    }
};

}  // namespace spectator
