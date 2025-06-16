#pragma once

#include <libs/meter/meter_types/include/meter.h>
#include <libs/meter/meter_id/meter_id.h>
#include <libs/writer/writer_wrapper/include/Writer.h>

#include <string>

static constexpr auto AGE_GAUGE_TYPE_SYMBOL = "A";

class AgeGauge final : public Meter
{
   public:
    explicit AgeGauge(const MeterId& meter_id) : Meter(meter_id, AGE_GAUGE_TYPE_SYMBOL) {}

    void Now()
    {
        auto line = this->m_meterTypeSymbol + FIELD_SEPARATOR + this->m_id.GetSpectatordId() + FIELD_SEPARATOR + "0";
        Writer::GetInstance().Write(line);
    }

    void Set(const int& seconds)
    {
        auto line = this->m_meterTypeSymbol + FIELD_SEPARATOR + this->m_id.GetSpectatordId() + FIELD_SEPARATOR +
                    std::to_string(seconds);
        Writer::GetInstance().Write(line);
    }
};
