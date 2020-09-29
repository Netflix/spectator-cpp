#pragma once

#include "meter.h"

namespace spectator {

class MaxGauge : public Meter {
 public:
  MaxGauge(IdPtr id, Publisher* publisher) : Meter(std::move(id), publisher) {}
  void Update(double value) noexcept { send(value); }
  // synonym for Update for consistency with the Gauge interface
  void Set(double value) noexcept { send(value); }

 protected:
  std::string_view Type() override { return "m"; }
};

}  // namespace spectator
