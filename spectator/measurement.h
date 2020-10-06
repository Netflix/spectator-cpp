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

template <>
struct fmt::formatter<spectator::Measurement> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  // formatter for Ids
  template <typename FormatContext>
  auto format(const spectator::Measurement& m, FormatContext& context) {
    return fmt::format_to(context.out(), "Measurement({}, {})", *(m.id),
                          m.value);
  }
};