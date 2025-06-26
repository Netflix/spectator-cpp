#pragma once

#include <libs/meter/meter_types/include/meter.h>
#include <libs/meter/meter_id/meter_id.h>
#include <libs/writer/writer_wrapper/writer.h>

#include <string>

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
