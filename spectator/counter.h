#pragma once

#include "meter.h"

namespace spectator {

class Counter : public Meter {
 public:
  Counter(IdPtr id, Publisher* publisher) : Meter(std::move(id), publisher) {}
  void Increment() noexcept { send(1); };
  void Add(double delta) noexcept { send(delta); }

 protected:
  std::string_view Type() override { return "c"; }
};

}  // namespace spectator
