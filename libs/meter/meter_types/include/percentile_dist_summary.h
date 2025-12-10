#pragma once

#include <meter.h>
#include <meter_id.h>
#include <writer.h>

#include <cstdint>
#include <string>

namespace spectator {

static constexpr auto PERCENTILE_DISTRIBUTION_SUMMARY_TYPE_SYMBOL = "D";

class PercentileDistributionSummary final : public Meter
{
   public:
    explicit PercentileDistributionSummary(const MeterId& meter_id)
        : Meter(meter_id, PERCENTILE_DISTRIBUTION_SUMMARY_TYPE_SYMBOL)
    {
    }

    void Record(const int64_t& amount) const 
    {
        if (amount >= 0)
        {
            auto line = this->ConstructLine(amount);
            Writer::GetInstance().Write(line);
        }
    }
};

}  // namespace spectator
