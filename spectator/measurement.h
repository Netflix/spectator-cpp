#pragma once

#include "id.h"
#include <ostream>

namespace spectator {

struct Measurement {
  IdPtr id;
  double value;

  bool operator==(const Measurement& other) const {
    return std::abs(value - other.value) < 1e-9 && *id == *(other.id);
  }
};

inline std::ostream& operator<<(std::ostream& os, const Measurement& m) {
  os << "Measurement{" << *(m.id) << "," << m.value << "}";
  return os;
}

}  // namespace spectator
