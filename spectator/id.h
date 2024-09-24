#pragma once

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <memory>
#include <ostream>
#include <string>

namespace spectator {

class Tags {
  using table_t = absl::flat_hash_map<std::string, std::string>;
  table_t entries_;

 public:
  Tags() = default;

  Tags(std::initializer_list<std::pair<absl::string_view, absl::string_view>> vs) {
    for (auto& pair : vs) {
      add(pair.first, pair.second);
    }
  }

  template <typename Cont>
  static Tags from(Cont&& cont) {
    Tags tags;
    tags.entries_.reserve(cont.size());
    for (auto&& kv : cont) {
      tags.add(kv.first, kv.second);
    }
    return tags;
  }

  void add(absl::string_view k, absl::string_view v) {
    entries_[k] = std::string(v);
  }

  [[nodiscard]] size_t hash() const {
    using hs = std::hash<std::string>;
    size_t h = 0;
    for (const auto& entry : entries_) {
      h += (hs()(entry.first) << 1U) ^ hs()(entry.second);
    }
    return h;
  }

  void move_all(Tags&& source) {
    entries_.insert(std::make_move_iterator(source.begin()),
                    std::make_move_iterator(source.end()));
  }

  bool operator==(const Tags& that) const { return that.entries_ == entries_; }

  [[nodiscard]] bool has(absl::string_view key) const {
    return entries_.find(key) != entries_.end();
  }

  [[nodiscard]] std::string at(absl::string_view key) const {
    auto entry = entries_.find(key);
    if (entry != entries_.end()) {
      return entry->second;
    }
    return {};
  }

  [[nodiscard]] size_t size() const { return entries_.size(); }

  [[nodiscard]] table_t::const_iterator begin() const {
    return entries_.begin();
  }

  [[nodiscard]] table_t::const_iterator end() const { return entries_.end(); }
};

inline std::ostream& operator<<(std::ostream& os, const Tags& tags) {
  os << fmt::format("{}", tags);
  return os;
}

class Id {
 public:
  Id(absl::string_view name, Tags tags) noexcept
      : name_(name), tags_(std::move(tags)), hash_(0u) {}

  static std::shared_ptr<Id> of(absl::string_view name, Tags tags = {}) {
    return std::make_shared<Id>(name, std::move(tags));
  }

  bool operator==(const Id& rhs) const noexcept {
    return name_ == rhs.name_ && tags_ == rhs.tags_;
  }

  const std::string& Name() const noexcept { return name_; }

  const Tags& GetTags() const noexcept { return tags_; }

  std::unique_ptr<Id> WithTag(const std::string& key,
                              const std::string& value) const {
    // Create a copy
    Tags tags{GetTags()};
    tags.add(key, value);
    return std::make_unique<Id>(Name(), tags);
  }

  std::unique_ptr<Id> WithTags(Tags&& extra_tags) const {
    Tags tags{GetTags()};
    tags.move_all(std::move(extra_tags));
    return std::make_unique<Id>(Name(), tags);
  }

  std::unique_ptr<Id> WithTags(const Tags& extra_tags) const {
    Tags tags{GetTags()};
    for (const auto& t : extra_tags) {
      tags.add(t.first, t.second);
    }
    return std::make_unique<Id>(Name(), tags);
  }

  std::unique_ptr<Id> WithStat(const std::string& stat) const {
    return WithTag("statistic", stat);
  };

  static std::shared_ptr<Id> WithDefaultStat(std::shared_ptr<Id> baseId,
                                             const std::string& stat) {
    if (baseId->GetTags().has("statistic")) {
      return baseId;
    } else {
      return baseId->WithStat(stat);
    }
  }

  friend struct std::hash<Id>;

  friend struct std::hash<std::shared_ptr<Id>>;

 private:
  std::string name_;
  Tags tags_;
  mutable size_t hash_;

  size_t Hash() const noexcept {
    if (hash_ == 0) {
      // compute hash code, and reuse it
      hash_ = tags_.hash() ^ std::hash<std::string>()(name_);
    }
    return hash_;
  }
};

using IdPtr = std::shared_ptr<Id>;

}  // namespace spectator

namespace std {

template <>
struct hash<spectator::Id> {
  size_t operator()(const spectator::Id& id) const { return id.Hash(); }
};

template <>
struct hash<spectator::Tags> {
  size_t operator()(const spectator::Tags& tags) const { return tags.hash(); }
};

template <>
struct hash<shared_ptr<spectator::Id>> {
  size_t operator()(const shared_ptr<spectator::Id>& id) const {
    return id->Hash();
  }
};

template <>
struct equal_to<shared_ptr<spectator::Id>> {
  bool operator()(const shared_ptr<spectator::Id>& lhs,
                  const shared_ptr<spectator::Id>& rhs) const {
    return *lhs == *rhs;
  }
};

}  // namespace std

template <>
struct fmt::formatter<spectator::Id> : fmt::formatter<std::string_view> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  // formatter for Ids
  template <typename FormatContext>
  auto format(const spectator::Id& id, FormatContext& context) {
    return fmt::format_to(context.out(), "Id(name={}, tags={})", id.Name(),
                          id.GetTags());
  }
};
