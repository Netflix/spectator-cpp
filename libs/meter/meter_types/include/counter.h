#pragma once

#include <libs/meter/meter_types/include/base/meter.h>
#include <libs/meter/meter_id/include/meter_id.h>
#include <libs/writer/writer_wrapper/include/Writer.h>

#include <string>

static constexpr auto COUNTER_TYPE_SYMBOL = "c";

class Counter final : public Meter
{
  public:
    explicit Counter(const MeterId &meter_id) : Meter(meter_id, COUNTER_TYPE_SYMBOL)
    {
    }

    void Increment(const double &delta = 1)
    {
        if (delta > 0)
        {
            auto line = this->m_meterTypeSymbol + FIELD_SEPARATOR + this->m_id.spectatord_id + FIELD_SEPARATOR + std::to_string(delta);
            Writer::GetInstance().Write(line);
        }
    }
};
