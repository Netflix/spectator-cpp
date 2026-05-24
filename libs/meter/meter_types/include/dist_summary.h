#pragma once

#include <meter.h>
#include <meter_id.h>

#include <string>

namespace spectator {

static constexpr auto DisTRIBUTION_SUMMARY_TYPE_SYMBOL = "d";

class DistributionSummary final : public Meter
{
   public:
    explicit DistributionSummary(MeterId meter_id) : Meter(std::move(meter_id), DisTRIBUTION_SUMMARY_TYPE_SYMBOL) {}

    void Record(const double& amount) const
    {
        if (amount >= 0)
        {
            this->ConstructLine(amount);
        }
    }
};

}  // namespace spectator