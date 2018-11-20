#include "id.h"
#include "memory.h"
#include <ostream>
#include <utility>

namespace spectator {

const std::string& Id::Name() const noexcept { return name_; }

const Tags& Id::GetTags() const noexcept { return tags_; }

bool Id::operator==(const Id& rhs) const noexcept {
  return name_ == rhs.name_ && tags_ == rhs.tags_;
}

std::unique_ptr<spectator::Id> Id::WithTag(const std::string& key,
                                           const std::string& value) const {
  // Create a copy
  Tags tags{GetTags()};
  tags.add(key, value);
  return std::make_unique<Id>(Name(), tags);
}

inline std::ostream& operator<<(std::ostream& os, const Tags& tags) {
  bool first = true;
  os << '[';
  for (const auto& tag : tags) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << tag.first << "->" << tag.second;
  }
  os << ']';
  return os;
}

std::ostream& operator<<(std::ostream& os, const Id& id) {
  os << "Id(" << id.Name() << ", " << id.GetTags() << ")";
  return os;
}

}  // namespace spectator
