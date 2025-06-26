#pragma once

#include <libs/meter/meter_types/include/meter.h>
#include <libs/meter/meter_id/meter_id.h>
#include <libs/writer/writer_wrapper/writer.h>

#include <string>

static constexpr auto TIMER_TYPE_SYMBOL = "t";

class Timer final : public Meter
{
   public:
    explicit Timer(const MeterId& meter_id) : Meter(meter_id, TIMER_TYPE_SYMBOL) {}

    void Record(const double& seconds) const
    {
        if (seconds >= 0)
        {
            auto line = this->ConstructLine(seconds);
            Writer::GetInstance().Write(line);
        }
    }
};