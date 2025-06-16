#pragma once

#include <libs/meter/meter_types/include/base/meter.h>
#include <libs/meter/meter_id/include/meter_id.h>
#include <libs/writer/writer_wrapper/include/Writer.h>

#include <string>

static constexpr auto DisTRIBUTION_SUMMARY_TYPE_SYMBOL = "d";

class DistributionSummary final : public Meter
{
   public:
    explicit DistributionSummary(const MeterId& meter_id) : Meter(meter_id, DisTRIBUTION_SUMMARY_TYPE_SYMBOL) {}

    void Record(const int& amount)
    {
        if (amount >= 0)
        {
            auto line = this->m_meterTypeSymbol + FIELD_SEPARATOR + this->m_id.GetSpectatordId() + FIELD_SEPARATOR +
                        std::to_string(amount);
            Writer::GetInstance().Write(line);
        }
    }
};