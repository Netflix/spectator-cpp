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

    const std::unordered_map<std::string, std::string> &GetTags() const noexcept { return m_tags; };

    MeterId WithTag(const std::string &key, const std::string &value) const;

    MeterId WithTags(const std::unordered_map<std::string, std::string> &additional_tags) const;

    std::string spectatord_id;

    bool operator==(const MeterId &other) const;

    std::string to_string() const;

    std::string GetSpectatordId() const
    {
        return ToSpectatorId(m_name, m_tags);
    }

  private:
    std::string RepleaceInvalidChars(const std::string &s) const;
    std::string ToSpectatorId(const std::string &name, const std::unordered_map<std::string, std::string> &tags) const;

    static const std::regex INVALID_CHARS;
    std::string m_name;
    std::unordered_map<std::string, std::string> m_tags;
};

namespace std
{
template <> struct hash<MeterId>
{
    size_t operator()(const MeterId &id) const;
};
} // namespace std