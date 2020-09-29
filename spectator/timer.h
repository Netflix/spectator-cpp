#pragma once
#include "meter.h"
#include "absl/time/time.h"
#include <chrono>

namespace spectator {

class Timer : public Meter {
 public:
  Timer(IdPtr id, Publisher* publisher) : Meter(std::move(id), publisher) {}
  void Record(std::chrono::nanoseconds amount) noexcept {
    Record(absl::FromChrono(amount));
  }

  void Record(absl::Duration amount) noexcept {
    auto secs = absl::ToDoubleSeconds(amount);
    send(secs);
  }

 protected:
  std::string_view Type() override { return "t"; }
};

}  // namespace spectator
