#pragma once

#include <libs/meter/meter_types/include/base/meter.h>
#include <libs/meter/meter_id/include/meter_id.h>
#include <libs/writer/writer_wrapper/include/Writer.h>

#include <string>

static constexpr auto PERCENTILE_DISTRIBUTION_SUMMARY_TYPE_SYMBOL = "D";

class PercentileDistributionSummary final : public Meter
{
  public:
    explicit PercentileDistributionSummary(const MeterId &meter_id)
        : Meter(meter_id, PERCENTILE_DISTRIBUTION_SUMMARY_TYPE_SYMBOL)
    {
    }

    void Record(const int &amount)
    {
        if (amount >= 0)
        {
            auto line = this->m_meterTypeSymbol + FIELD_SEPARATOR + this->m_id.spectatord_id + FIELD_SEPARATOR + std::to_string(amount);
            Writer::GetInstance().Write(line);
        }
    }
};
