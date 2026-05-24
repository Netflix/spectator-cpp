#pragma once

#include <string>
#include <map>
#include <memory>
#include <regex>
#include <functional>
#include <unordered_map>

namespace spectator {

struct ExtraCommonTags
{
    std::unordered_map<std::string, std::string> tags;
    std::string prefixString;
};

class MeterId
{
   public:
    MeterId(const std::string& name, const std::unordered_map<std::string, std::string>& tags = {}, std::shared_ptr<const ExtraCommonTags> extra = nullptr);

    const std::string& GetName() const noexcept { return m_name; };
    const std::string& GetSpectatordId() const noexcept { return m_spectatord_id; }
    const std::unordered_map<std::string, std::string>& GetTags() const noexcept { return m_tags; }

    MeterId WithTag(const std::string& key, const std::string& value) const;

    MeterId WithTags(const std::unordered_map<std::string, std::string>& additional_tags) const;

    bool operator==(const MeterId& other) const noexcept
    {
        return m_name == other.m_name && m_tags == other.m_tags;
    }
    std::string to_string() const;

   private:
    MeterId() = default;

    std::string m_name;
    std::unordered_map<std::string, std::string> m_tags;
    std::string m_spectatord_id;
};

}  // namespace spectator

namespace std {
template <>
struct hash<spectator::MeterId>
{
    size_t operator()(const spectator::MeterId& id) const noexcept
    {
        return hash<string>{}(id.GetSpectatordId());
    }
};
}  // namespace std
