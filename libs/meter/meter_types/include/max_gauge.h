#pragma once

#include <libs/meter/meter_types/include/base/meter.h>
#include <libs/meter/meter_id/meter_id.h>
#include <libs/writer/writer_wrapper/include/Writer.h>

#include <string>

static constexpr auto MAX_GAUGE_TYPE_SYMBOL = "m";

class MaxGauge final : public Meter
{
   public:
    explicit MaxGauge(const MeterId& meter_id) : Meter(meter_id, MAX_GAUGE_TYPE_SYMBOL) {}

    void Set(const double& value)
    {
        auto line = this->m_meterTypeSymbol + FIELD_SEPARATOR + this->m_id.GetSpectatordId() + FIELD_SEPARATOR +
                    std::to_string(value);
        Writer::GetInstance().Write(line);
    }
};