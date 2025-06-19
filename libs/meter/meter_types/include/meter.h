#pragma once

#include <libs/meter/meter_id/meter_id.h>
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
    
    template <typename T>
    inline std::string ConstructLine(const T& value) const
    {
        // Pre-calculate the required size to avoid reallocations
        const auto& id_str = m_id.GetSpectatordId();
        const auto value_str = std::to_string(value);
        std::string result;
        result.reserve(m_meterTypeSymbol.size() + id_str.size() + value_str.size() + 2); // +2 for two separators
        
        // Build the string with append operations (more efficient than + operator)
        result.append(m_meterTypeSymbol);
        result.append(FIELD_SEPARATOR);
        result.append(id_str);
        result.append(FIELD_SEPARATOR);
        result.append(value_str);
        
        return result;
    }

   protected:
    MeterId m_id;
    std::string m_meterTypeSymbol;
};
