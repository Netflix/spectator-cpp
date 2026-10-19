#include <meter_id.h>

#include <util.h>

#include <array>
#include <cstdint>
#include <map>
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

// A tag is published only if neither its key nor its value is empty or whitespace-only. ValidateTags (GetTags()) and
// ToSpectatorId (the wire id) both use this, so the two always agree.
bool IsValidTag(const std::string& key, const std::string& value)
{
    return IsEmptyOrWhitespace(key) == false && IsEmptyOrWhitespace(value) == false;
}

std::unordered_map<std::string, std::string> ValidateTags(const std::unordered_map<std::string, std::string>& tags)
{
    std::unordered_map<std::string, std::string> validTags{};

    for (const auto& [key, value] : tags)
    {
        if (IsValidTag(key, value))
        {
            validTags[key] = value;
        }
    }
    return validTags;
}

// all_tags_valid is true when ValidateTags dropped nothing, so the common case renders the map without re-checking
// every tag. Otherwise the tags ValidateTags dropped are skipped here too.
std::string ToSpectatorId(const std::string& name, const std::unordered_map<std::string, std::string>& tags,
                          bool all_tags_valid)
{
    // Upper bound: it also counts tags skipped below, which only over-reserves when a tag is invalid.
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
        if (all_tags_valid == false && IsValidTag(key, value) == false)
        {
            continue;
        }
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
    // The id must not contain the tags ValidateTags dropped: rendering every raw tag would publish e.g. "k=" or "_=v"
    // while GetTags() reports neither. It still walks the caller's map, not m_tags, so the tag order on the wire is
    // unchanged for valid input. Keys are unique, so equal sizes mean nothing was dropped.
    m_spectatord_id = ToSpectatorId(m_name, tags, m_tags.size() == tags.size());
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
    // m_tags is unordered; sort so the output does not depend on the standard library's iteration order.
    const std::map<std::string, std::string> sorted_tags(m_tags.begin(), m_tags.end());

    std::ostringstream ss;
    ss << "MeterId(name=" << m_name << ", tags={";
    bool first = true;
    for (const auto& [fst, snd] : sorted_tags)
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

namespace {

// splitmix64 finalizer. Spreads every input bit over the whole word so that summing many pair hashes below does not
// cancel or cluster structure from the individual std::hash<std::string> values.
constexpr uint64_t Mix(uint64_t x)
{
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

constexpr uint64_t kGoldenRatio = 0x9e3779b97f4a7c15ULL;

}  // namespace

// Implementation of the hash function for MeterId.
// operator== treats the tags as an unordered map, so equal ids can iterate their tags in different orders (different
// insertion order or bucket count). The hash must not depend on that order: each tag pair is hashed on its own and the
// results are summed, and addition is commutative.
size_t std::hash<spectator::MeterId>::operator()(const spectator::MeterId& id) const
{
    const std::hash<std::string> hash_string;

    uint64_t tags_sum = 0;
    for (const auto& [key, value] : id.GetTags())
    {
        // The multiply makes the pair hash differ for {a: b} and {b: a}. Mixing each pair before the sum keeps a key
        // associated with its own value; summing raw key and value hashes would make {a: 1, b: 2} and {a: 2, b: 1}
        // collide.
        tags_sum += Mix(hash_string(key) + hash_string(value) * kGoldenRatio);
    }

    return static_cast<size_t>(Mix(hash_string(id.GetName()) + kGoldenRatio + tags_sum));
}
