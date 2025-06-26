#pragma once

#include <libs/meter/meter_types/include/meter.h>
#include <libs/meter/meter_id/meter_id.h>
#include <libs/writer/writer_wrapper/writer.h>

#include <string>

static constexpr auto AGE_GAUGE_TYPE_SYMBOL = "A";

class AgeGauge final : public Meter
{
   public:
    explicit AgeGauge(const MeterId& meter_id) : Meter(meter_id, AGE_GAUGE_TYPE_SYMBOL) {}

    void Now() const
    {
        auto line = this->ConstructLine(0);
        Writer::GetInstance().Write(line);
    }

    void Set(const int& seconds) const
    {
        auto line = this->ConstructLine(seconds);
        Writer::GetInstance().Write(line);
    }
};
