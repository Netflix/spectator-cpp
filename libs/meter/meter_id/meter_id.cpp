#include <meter_id.h>

#include <util.h>

#include <array>
#include <sstream>

namespace spectator {

namespace {

// spectatord accepts only -._A-Za-z0-9~^ in names, tag keys and tag values; every other byte becomes '_'.
// Built at compile time so sanitizing is one table lookup per byte (this replaces a std::regex_replace per string).
constexpr std::array<bool, 256> MakeValidCharTable()
{
    std::array<bool, 256> table{};
    for (char c = 'A'; c <= 'Z'; ++c) table[static_cast<unsigned char>(c)] = true;
    for (char c = 'a'; c <= 'z'; ++c) table[static_cast<unsigned char>(c)] = true;
    for (char c = '0'; c <= '9'; ++c) table[static_cast<unsigned char>(c)] = true;
    for (const char c : {'-', '.', '_', '~', '^'}) table[static_cast<unsigned char>(c)] = true;
    return table;
}

constexpr std::array<bool, 256> kValidChars = MakeValidCharTable();

// Appends src to dest with invalid characters replaced by '_'. Grows dest once instead of once per character.
void AppendSanitized(std::string& dest, const std::string& src)
{
    const size_t start = dest.size();
    dest.resize(start + src.size());
    char* out = dest.data() + start;
    for (const char c : src)
    {
        *out++ = kValidChars[static_cast<unsigned char>(c)] ? c : '_';
    }
}

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

std::string ToSpectatorId(const std::string& name, const std::unordered_map<std::string, std::string>& tags)
{
    size_t size = name.size();
    for (const auto& [key, value] : tags)
    {
        size += key.size() + value.size() + 2;  // ',' and '='
    }

    std::string id;
    id.reserve(size);
    AppendSanitized(id, name);
    for (const auto& [key, value] : tags)
    {
        id += ',';
        AppendSanitized(id, key);
        id += '=';
        AppendSanitized(id, value);
    }
    return id;
}

}  // namespace

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
