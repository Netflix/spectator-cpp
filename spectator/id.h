#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <ska/flat_hash_map.hpp>

namespace spectator {

class Tags {
  using K = std::string;
  using V = std::string;
  using table_t = ska::flat_hash_map<K, V>;
  table_t entries_;

 public:
  Tags() = default;

  Tags(std::initializer_list<std::pair<std::string, std::string>> vs) {
    for (auto& pair : vs) {
      add(std::move(pair.first), std::move(pair.second));
    }
  }

  void add(K k, V v) { entries_[std::move(k)] = std::move(v); }

  size_t hash() const {
    size_t h = 0;
    for (const auto& entry : entries_) {
      h += (std::hash<K>()(entry.first) << 1) ^ std::hash<V>()(entry.second);
    }
    return h;
  }

  void add_all(const Tags& source) {
    for (const auto& t : source.entries_) {
      entries_[t.first] = t.second;
    }
  }

  bool operator==(const Tags& that) const { return that.entries_ == entries_; }

  bool has(const K& key) const { return entries_.find(key) != entries_.end(); }

  K at(const K& key) const {
    auto entry = entries_.find(key);
    if (entry != entries_.end()) {
      return entry->second;
    }
    return {};
  }

  size_t size() const { return entries_.size(); }

  table_t::const_iterator begin() const { return entries_.begin(); }

  table_t::const_iterator end() const { return entries_.end(); }
};

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

class Id {
 public:
  Id(std::string name, Tags tags) noexcept
      : name_(std::move(name)), tags_(std::move(tags)), hash_(0u) {}

  bool operator==(const Id& rhs) const noexcept;

  const std::string& Name() const noexcept;

  const Tags& GetTags() const noexcept;

  std::unique_ptr<Id> WithTag(const std::string& key,
                              const std::string& value) const;

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
