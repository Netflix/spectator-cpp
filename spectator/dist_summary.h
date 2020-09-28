#pragma once
#include <utility>

#include "meter.h"

namespace spectator {

class DistributionSummary : public Meter {
 public:
  DistributionSummary(IdPtr id, Publisher* publisher)
      : Meter(std::move(id), publisher) {}
  void Record(double amount) noexcept { send(amount); }

 protected:
  std::string_view Type() override { return "d"; }
};

}  // namespace spectator
