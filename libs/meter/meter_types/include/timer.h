#pragma once

#include <libs/meter/meter_types/include/base/meter.h>
#include <libs/meter/meter_id/include/meter_id.h>
#include <libs/writer/writer_wrapper/include/Writer.h>

#include <string>

static constexpr auto TIMER_TYPE_SYMBOL = "t";

class Timer final : public Meter
{
   public:
    explicit Timer(const MeterId& meter_id) : Meter(meter_id, TIMER_TYPE_SYMBOL) {}

    void Record(const double& seconds)
    {
        if (seconds >= 0)
        {
            auto line = this->m_meterTypeSymbol + FIELD_SEPARATOR + this->m_id.GetSpectatordId() + FIELD_SEPARATOR +
                        std::to_string(seconds);
            Writer::GetInstance().Write(line);
        }
    }
};