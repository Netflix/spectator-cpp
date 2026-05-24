#pragma once

#include <meter.h>
#include <meter_id.h>

#include <string>

namespace spectator {

static constexpr auto AGE_GAUGE_TYPE_SYMBOL = "A";

class AgeGauge final : public Meter
{
   public:
    explicit AgeGauge(MeterId meter_id) : Meter(std::move(meter_id), AGE_GAUGE_TYPE_SYMBOL) {}

    void Now() const
    {
        this->ConstructLine(0);
    }

    void Set(const double& seconds) const
    {
        this->ConstructLine(seconds);
    }
};

}  // namespace spectator
