#include "meter.h"
#include "publisher.h"

namespace spectator {

namespace {

std::string create_prefix(const Id& id, std::string_view type_name) {
  std::string res = absl::StrCat("1:", type_name, ":", id.Name());
  auto first = true;
  for (const auto& tags : id.GetTags()) {
    if (first) {
      absl::StrAppend(&res, ":#", tags.first, "=", tags.second);
      first = false;
    } else {
      absl::StrAppend(&res, ",", tags.first, "=", tags.second);
    }
  }
  absl::StrAppend(&res, ":");
  return res;
}

}  // namespace

Meter::Meter(IdPtr id, Publisher* publisher)
    : id_(std::move(id)), publisher_(publisher) {}

void Meter::send(double value) {
  if (value_prefix_.empty()) {
    value_prefix_ = create_prefix(*id_, Type());
  }
  auto msg = absl::StrCat(value_prefix_, value);
  publisher_->send(msg);
}

}  // namespace spectator