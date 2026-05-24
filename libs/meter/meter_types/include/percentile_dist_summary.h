#pragma once

#include <meter.h>
#include <meter_id.h>

#include <cstdint>
#include <string>

namespace spectator {

static constexpr auto PERCENTILE_DISTRIBUTION_SUMMARY_TYPE_SYMBOL = "D";

class PercentileDistributionSummary final : public Meter
{
   public:
    explicit PercentileDistributionSummary(MeterId meter_id)
        : Meter(std::move(meter_id), PERCENTILE_DISTRIBUTION_SUMMARY_TYPE_SYMBOL)
    {
    }

    void Record(const int64_t& amount) const 
    {
        if (amount >= 0)
        {
            this->ConstructLine(amount);
        }
    }
};

}  // namespace spectator
