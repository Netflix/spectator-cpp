#pragma once

#include "id.h"
#include <fmt/format.h>

namespace spectator {

struct Measurement {
  IdPtr id;
  double value;

  bool operator==(const Measurement& other) const {
    return std::abs(value - other.value) < 1e-9 && *id == *(other.id);
  }

  Measurement(IdPtr idPtr, double v) : id(std::move(idPtr)), value(v) {}
};

}  // namespace spectator

template <> struct fmt::formatter<spectator::Measurement>: formatter<std::string_view> {
  static auto format(const spectator::Measurement& m, format_context& ctx) -> format_context::iterator {
    return fmt::format_to(ctx.out(), "Measurement({}, {})", *(m.id), m.value);
  }
};
