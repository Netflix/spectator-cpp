#include "id.h"
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

std::unique_ptr<Id> Id::WithTags(Tags&& extra_tags) const {
  Tags tags{GetTags()};
  tags.move_all(std::move(extra_tags));
  return std::make_unique<Id>(Name(), tags);
}

std::ostream& operator<<(std::ostream& os, const Id& id) {
  os << fmt::format("{}", id);
  return os;
}

}  // namespace spectator
