#pragma once

#include <libs/meter/meter_id/include/meter_id.h>
#include <string>

class Meter
{
   public:
    static constexpr auto FIELD_SEPARATOR = ":";

    Meter(const MeterId& meter_id, const std::string& meter_type_symbol)
        : m_id(meter_id), m_meterTypeSymbol(meter_type_symbol)
    {
    }
    virtual ~Meter() = default;

    const MeterId& GetId() const noexcept { return m_id; }

    const std::string& GetMeterTypeSymbol() const noexcept { return m_meterTypeSymbol; }

   protected:
    MeterId m_id;
    std::string m_meterTypeSymbol;
};
