#pragma once

#include <meter.h>
#include <meter_id.h>
#include <writer.h>

#include <string>

static constexpr auto MAX_GAUGE_TYPE_SYMBOL = "m";

class MaxGauge final : public Meter
{
   public:
    explicit MaxGauge(const MeterId& meter_id) : Meter(meter_id, MAX_GAUGE_TYPE_SYMBOL) {}

    void Set(const double& value) const
    {
        auto line = this->ConstructLine(value);
        Writer::GetInstance().Write(line);
    }
};