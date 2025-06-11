#pragma once

#include <string>
#include <map>
#include <regex>
#include <functional>
#include <unordered_map>

class MeterId
{
  public:
    MeterId(const std::string &name, const std::unordered_map<std::string, std::string> &tags = {});

    const std::string &GetName() const noexcept { return m_name; };
    const std::string &GetSpectatordId() const noexcept { return m_spectatord_id; }
    const std::unordered_map<std::string, std::string> &GetTags() const noexcept { return m_tags; };

    MeterId WithTag(const std::string &key, const std::string &value) const;

    MeterId WithTags(const std::unordered_map<std::string, std::string> &additional_tags) const;

    bool operator==(const MeterId &other) const;

    std::string to_string() const;

  private:
    std::string m_name;
    std::unordered_map<std::string, std::string> m_tags;
    std::string m_spectatord_id;
};


template <> struct std::hash<MeterId>
{
    size_t operator()(const MeterId &id) const;
};
