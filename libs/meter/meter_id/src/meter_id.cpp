#include <libs/meter/meter_id/include/meter_id.h>

#include <format>
#include <sstream>

// Define the static member
const std::regex INVALID_CHARS("[^-._A-Za-z0-9~^]");

std::unordered_map<std::string, std::string> ValidateTags(const std::unordered_map<std::string, std::string>& tags)
{
    std::unordered_map<std::string, std::string> validTags{};

    for (const auto& [key, value] : tags)
    {
        if (key.empty() == false && value.empty() == false)
        {
            validTags[key] = value;
        }
    }

    return validTags;
}

std::string RepleaceInvalidChars(const std::string& s) { return std::regex_replace(s, INVALID_CHARS, "_"); }

std::string ToSpectatorId(const std::string& name, const std::unordered_map<std::string, std::string>& tags)
{
    std::ostringstream ss;
    ss << RepleaceInvalidChars(name);
    if (!tags.empty())
    {
        for (const auto& tag : tags)
        {
            ss << "," << RepleaceInvalidChars(tag.first) << "=" << RepleaceInvalidChars(tag.second);
        }
    }
    return ss.str();
}

MeterId::MeterId(const std::string& name, const std::unordered_map<std::string, std::string>& tags)
    : m_name(name), m_tags(ValidateTags(tags))
{
    m_spectatord_id = ToSpectatorId(name, tags);
}

MeterId MeterId::WithTag(const std::string& key, const std::string& value) const
{
    auto new_tags = m_tags;
    new_tags[key] = value;
    return MeterId(m_name, new_tags);
}

MeterId MeterId::WithTags(const std::unordered_map<std::string, std::string>& additional_tags) const
{
    auto new_tags = m_tags;
    for (const auto& pair : additional_tags)
    {
        new_tags[pair.first] = pair.second;
    }
    return MeterId(m_name, new_tags);
}

bool MeterId::operator==(const MeterId& other) const { return m_name == other.m_name && m_tags == other.m_tags; }

std::string MeterId::to_string() const
{
    std::ostringstream ss;
    ss << "MeterId(name=" << m_name << ", tags={";
    bool first = true;
    for (const auto& pair : m_tags)
    {
        if (!first)
        {
            ss << ", ";
        }
        ss << "'" << pair.first << "': '" << pair.second << "'";
        first = false;
    }
    ss << "})";
    return ss.str();
}

// Implementation of the hash function for MeterId
size_t std::hash<MeterId>::operator()(const MeterId& id) const
{
    // Hash the name first
    size_t name_hash = std::hash<std::string>{}(id.GetName());

    // Hash the tags
    size_t tags_hash = 0;
    for (const auto& tag : id.GetTags())
    {
        // Combine key and value hashes
        size_t pair_hash = std::hash<std::string>{}(tag.first) ^ (std::hash<std::string>{}(tag.second) << 1);
        // Combine with the accumulated tags hash
        tags_hash ^= pair_hash + 0x9e3779b9 + (tags_hash << 6) + (tags_hash >> 2);
    }

    // Combine name hash and tags hash
    return name_hash ^ (tags_hash << 1);
}
