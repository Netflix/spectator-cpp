#pragma once

#include "meter.h"

namespace spectator {
class MonotonicCounter : public Meter {
 public:
  MonotonicCounter(IdPtr id, Publisher* publisher)
      : Meter(std::move(id), publisher) {}
  void Set(double amount) noexcept { send(amount); }

 protected:
  std::string_view Type() override { return "C"; }
};
}  // namespace spectator
