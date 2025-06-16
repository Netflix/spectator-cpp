#pragma once

#include <libs/meter/meter_types/include/base/meter.h>
#include <libs/meter/meter_id/meter_id.h>
#include <libs/writer/writer_wrapper/include/Writer.h>

#include <string>

static constexpr auto MONOTONIC_COUNTER_UINT_TYPE_SYMBOL = "U";

class MonotonicCounterUint final : public Meter
{
   public:
    explicit MonotonicCounterUint(const MeterId& meter_id) : Meter(meter_id, MONOTONIC_COUNTER_UINT_TYPE_SYMBOL) {}

    void Set(const uint64_t& amount)
    {
        auto line = this->m_meterTypeSymbol + FIELD_SEPARATOR + this->m_id.GetSpectatordId() + FIELD_SEPARATOR +
                    std::to_string(amount);
        Writer::GetInstance().Write(line);
    }
};
