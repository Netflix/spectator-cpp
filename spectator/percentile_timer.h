#pragma once

#include "meter.h"
#include "util.h"
#include "absl/time/time.h"
#include <chrono>

namespace spectator {

class PercentileTimer : public Meter {
 public:
  PercentileTimer(IdPtr id, Publisher* publisher, absl::Duration min,
                  absl::Duration max)
      : Meter(std::move(id), publisher), min_(min), max_(max) {}

  PercentileTimer(IdPtr id, Publisher* publisher, std::chrono::nanoseconds min,
                  std::chrono::nanoseconds max)
      : PercentileTimer(std::move(id), publisher, absl::FromChrono(min),
                        absl::FromChrono(max)) {}

  void Record(std::chrono::nanoseconds amount) noexcept {
    Record(absl::FromChrono(amount));
  }

  void Record(absl::Duration amount) noexcept {
    auto duration = restrict(amount, min_, max_);
    send(absl::ToDoubleSeconds(duration));
  }

 protected:
  std::string_view Type() override { return "T"; }

 private:
  absl::Duration min_;
  absl::Duration max_;
};

}  // namespace spectator
