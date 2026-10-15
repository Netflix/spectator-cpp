#include <meter_id.h>

#include <util.h>
#include <sstream>

namespace spectator {

namespace {

// The characters spectatord accepts in names and tags: [-._A-Za-z0-9~^]
bool IsValidChar(const char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
           c == '_' || c == '~' || c == '^';
}

}  // namespace

std::unordered_map<std::string, std::string> ValidateTags(const std::unordered_map<std::string, std::string>& tags)
{
    std::unordered_map<std::string, std::string> validTags{};

    for (const auto& [key, value] : tags)
    {
        if (IsEmptyOrWhitespace(key) == false && IsEmptyOrWhitespace(value) == false)
        {
            validTags[key] = value;
        }
    }
    return validTags;
}

// Appends s with each invalid byte replaced by '_'. A byte loop rather than std::regex_replace, which was the dominant
// cost of creating a MeterId.
void AppendValidChars(std::string& out, const std::string& s)
{
    for (const char c : s)
    {
        out += IsValidChar(c) ? c : '_';
    }
}

std::string ToSpectatorId(const std::string& name, const std::unordered_map<std::string, std::string>& tags)
{
    // Each byte is kept or replaced by one byte, so the size is known up front and the id is allocated once.
    std::size_t size = name.size();
    for (const auto& [fst, snd] : tags)
    {
        size += 1 + fst.size() + 1 + snd.size();
    }

    std::string id;
    id.reserve(size);
    AppendValidChars(id, name);
    for (const auto& [fst, snd] : tags)
    {
        id += ',';
        AppendValidChars(id, fst);
        id += '=';
        AppendValidChars(id, snd);
    }
    return id;
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
    for (const auto& [fst, snd] : additional_tags)
    {
        new_tags[fst] = snd;
    }
    return MeterId(m_name, new_tags);
}

MeterId MeterId::WithStat(const std::string& stat) const
{
    return WithTag("statistic", stat);
}

bool MeterId::operator==(const MeterId& other) const { return m_name == other.m_name && m_tags == other.m_tags; }

std::string MeterId::to_string() const
{
    std::ostringstream ss;
    ss << "MeterId(name=" << m_name << ", tags={";
    bool first = true;
    for (const auto& [fst, snd] : m_tags)
    {
        if (!first)
        {
            ss << ", ";
        }
        ss << "'" << fst << "': '" << snd << "'";
        first = false;
    }
    ss << "})";
    return ss.str();
}

}  // namespace spectator

// Implementation of the hash function for MeterId
size_t std::hash<spectator::MeterId>::operator()(const spectator::MeterId& id) const
{
    // Hash the name first
    const size_t name_hash = std::hash<std::string>{}(id.GetName());

    // Hash the tags
    size_t tags_hash = 0;
    for (const auto& [fst, snd] : id.GetTags())
    {
        // Combine key and value hashes
        const size_t pair_hash = std::hash<std::string>{}(fst) ^ std::hash<std::string>{}(snd) << 1;
        // Combine with the accumulated tags hash
        tags_hash ^= pair_hash + 0x9e3779b9 + (tags_hash << 6) + (tags_hash >> 2);
    }

    // Combine name hash and tags hash
    return name_hash ^ tags_hash << 1;
}
