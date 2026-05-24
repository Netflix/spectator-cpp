#pragma once

#include <meter_id.h>
#include <writer.h>

#include <string>

namespace spectator {

class Meter
{
   public:
    static constexpr auto FIELD_SEPARATOR = ":";

    Meter(MeterId meter_id, const std::string& meter_type_symbol)
        : m_id(std::move(meter_id))
    {
        m_line = meter_type_symbol + FIELD_SEPARATOR + m_id.GetSpectatordId() + FIELD_SEPARATOR;
        m_prefixSize = m_line.size();
        m_line.reserve(m_prefixSize + 32);
    }
    virtual ~Meter() = default;

    const MeterId& GetId() const noexcept { return m_id; }

    template <typename T>
    inline void ConstructLine(const T& value) const
    {
        m_line.replace(m_prefixSize, std::string::npos, std::to_string(value));
        Writer::GetInstance().Write(m_line);
    }

   protected:
    MeterId m_id;
    size_t m_prefixSize{};
    mutable std::string m_line;
};

}  // namespace spectator
