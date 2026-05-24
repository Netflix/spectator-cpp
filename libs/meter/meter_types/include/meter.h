#pragma once

#include <meter_id.h>
#include <writer.h>

#include <charconv>
#include <string>
#include <type_traits>

namespace spectator {

class Meter
{
   public:
    static constexpr auto FIELD_SEPARATOR = ":";

    Meter(MeterId meter_id, const std::string& meter_type_symbol)
        : m_id(std::move(meter_id))
    {
        m_line.reserve(meter_type_symbol.size() + m_id.GetSpectatordId().size() + 26);
        m_line = meter_type_symbol;
        m_line += FIELD_SEPARATOR;
        m_line += m_id.GetSpectatordId();
        m_line += FIELD_SEPARATOR;
        m_prefixSize = m_line.size();
    }
    virtual ~Meter() = default;

    const MeterId& GetId() const noexcept { return m_id; }

    template <typename T>
    inline void ConstructLine(const T& value) const
    {
        m_line.resize(m_prefixSize);
        char buf[32];
        char* end;
        if constexpr (std::is_floating_point_v<T>)
        {
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::fixed, 6);
            end = ptr;
        }
        else
        {
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
            end = ptr;
        }
        m_line.append(buf, end);
        m_line += '\n';
        Writer::GetInstance().Write(m_line);
        m_line.pop_back();
    }

   protected:
    MeterId m_id;
    size_t m_prefixSize{};
    mutable std::string m_line;
};

}  // namespace spectator
