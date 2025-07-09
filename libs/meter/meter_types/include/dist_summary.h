#pragma once

#include <meter.h>
#include <meter_id.h>
#include <writer.h>

#include <string>

static constexpr auto DisTRIBUTION_SUMMARY_TYPE_SYMBOL = "d";

class DistributionSummary final : public Meter
{
   public:
    explicit DistributionSummary(const MeterId& meter_id) : Meter(meter_id, DisTRIBUTION_SUMMARY_TYPE_SYMBOL) {}

    void Record(const double& amount) const
    {
        if (amount >= 0)
        {
            auto line = this->ConstructLine(amount);
            Writer::GetInstance().Write(line);
        }
    }
};