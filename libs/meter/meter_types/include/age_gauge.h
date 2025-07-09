#pragma once

#include <meter.h>
#include <meter_id.h>
#include <writer.h>

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

    void Set(const double& seconds) const
    {
        auto line = this->ConstructLine(seconds);
        Writer::GetInstance().Write(line);
    }
};
