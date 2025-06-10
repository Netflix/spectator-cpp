#pragma once

#include <libs/meter/meter_types/include/base/meter.h>
#include <libs/meter/meter_id/include/meter_id.h>
#include <libs/writer/writer_wrapper/include/Writer.h>

#include <string>

static constexpr auto MONOTONIC_COUNTER_TYPE_SYMBOL = "C";

class MonotonicCounter final : public Meter
{
  public:
    explicit MonotonicCounter(const MeterId &meter_id) : Meter(meter_id, MONOTONIC_COUNTER_TYPE_SYMBOL)
    {
    }

    void Set(const double &amount)
    {
        auto line = this->m_meterTypeSymbol + FIELD_SEPARATOR + this->m_id.spectatord_id + FIELD_SEPARATOR + std::to_string(amount);
        Writer::GetInstance().Write(line);
    }
};
