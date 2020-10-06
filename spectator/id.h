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

  Tags(
      std::initializer_list<std::pair<std::string_view, std::string_view>> vs) {
    for (auto& pair : vs) {
      add(pair.first, pair.second);
    }
  }

  void add(std::string_view k, std::string_view v) {
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

  [[nodiscard]] bool has(std::string_view key) const {
    return entries_.find(key) != entries_.end();
  }

  [[nodiscard]] std::string at(std::string_view key) const {
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
  Id(std::string_view name, Tags tags) noexcept
      : name_(name), tags_(std::move(tags)), hash_(0u) {}

  static std::shared_ptr<Id> of(std::string_view name, Tags tags = {}) {
    return std::make_shared<Id>(name, std::move(tags));
  }

  bool operator==(const Id& rhs) const noexcept;

  const std::string& Name() const noexcept;

  const Tags& GetTags() const noexcept;

  std::unique_ptr<Id> WithTag(const std::string& key,
                              const std::string& value) const;

  std::unique_ptr<Id> WithTags(Tags&& extra_tags) const;

  std::unique_ptr<Id> WithTags(const Tags& extra_tags) const;

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

  friend std::ostream& operator<<(std::ostream& os, const Id& id);

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
struct fmt::formatter<spectator::Id> {
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
