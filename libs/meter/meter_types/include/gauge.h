#pragma once

#include <meter.h>
#include <meter_id.h>
#include <writer.h>

#include <string>
#include <optional>

static constexpr auto GAUGE_TYPE_SYMBOL = "g";

class Gauge final : public Meter
{
   public:
    explicit Gauge(const MeterId& meter_id, const std::optional<int>& ttl_seconds = std::nullopt)
        : Meter(meter_id, ttl_seconds.has_value()
                              ? GAUGE_TYPE_SYMBOL + std::string(",") + std::to_string(ttl_seconds.value())
                              : GAUGE_TYPE_SYMBOL)
    {
    }

    void Set(const double& value) const
    {
        auto line = this->ConstructLine(value);
        Writer::GetInstance().Write(line);
    }
};