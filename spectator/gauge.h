#pragma once

#include <utility>

#include "meter.h"

namespace spectator {

class Gauge : public Meter {
 public:
  Gauge(IdPtr id, Publisher* publisher) : Meter(std::move(id), publisher) {}
  void Set(double value) noexcept { send(value); }

 protected:
  std::string_view Type() override { return "g"; }
};

}  // namespace spectator
