#pragma once

#include <meter.h>
#include <meter_id.h>
#include <writer.h>

#include <string>

static constexpr auto MONOTONIC_COUNTER_UINT_TYPE_SYMBOL = "U";

class MonotonicCounterUint final : public Meter
{
   public:
    explicit MonotonicCounterUint(const MeterId& meter_id) : Meter(meter_id, MONOTONIC_COUNTER_UINT_TYPE_SYMBOL) {}

    void Set(const uint64_t& amount) const
    {
        auto line = this->ConstructLine(amount);
        Writer::GetInstance().Write(line);
    }
};
