#pragma once

#include <meter.h>
#include <meter_id.h>

#include <string>

namespace spectator {

static constexpr auto MAX_GAUGE_TYPE_SYMBOL = "m";

class MaxGauge final : public Meter
{
   public:
    explicit MaxGauge(MeterId meter_id) : Meter(std::move(meter_id), MAX_GAUGE_TYPE_SYMBOL) {}

    void Set(const double& value) const
    {
        this->ConstructLine(value);
    }
};

}  // namespace spectator