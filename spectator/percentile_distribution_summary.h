#pragma once

#include "meter.h"
#include "util.h"

namespace spectator {

class PercentileDistributionSummary : public Meter {
 public:
  PercentileDistributionSummary(IdPtr id, Publisher* publisher, int64_t min,
                                int64_t max)
      : Meter(std::move(id), publisher), min_{min}, max_{max} {}

  void Record(int64_t amount) noexcept { send(restrict(amount, min_, max_)); }

 protected:
  std::string_view Type() override { return "D"; }

 private:
  int64_t min_;
  int64_t max_;
};

}  // namespace spectator
