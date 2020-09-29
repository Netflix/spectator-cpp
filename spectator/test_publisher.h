#pragma once

#include "publisher.h"
#include <string>
#include <vector>

namespace spectator {
class TestPublisher : public Publisher {
 public:
  TestPublisher() : Publisher("disabled") {
    sender_ = [this](std::string_view m) { measurements_.emplace_back(m); };
  }

  void Reset() { measurements_.clear(); }

  std::vector<std::string> Measurements() { return measurements_; }

 private:
  std::vector<std::string> measurements_;
};
}  // namespace spectator