#pragma once

#include <meter.h>
#include <meter_id.h>

#include <string>

namespace spectator {

static constexpr auto COUNTER_TYPE_SYMBOL = "c";

class Counter final : public Meter
{
   public:
    explicit Counter(MeterId meter_id) : Meter(std::move(meter_id), COUNTER_TYPE_SYMBOL) {}

    void Increment() const
    {
        m_line.resize(m_prefixSize);
        m_line += '1';
        m_line += '\n';
        this->WriteLine();
        m_line.pop_back();
    }

    void Increment(int64_t delta) const
    {
        if (delta > 0)
        {
            this->ConstructLine(delta);
        }
    }
};

}  // namespace spectator
