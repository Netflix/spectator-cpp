#include <meter_id.h>

#include <util.h>

#include <sstream>

namespace spectator {

static std::string Sanitize(const std::string& str)
{
    std::string result;
    result.reserve(str.size());
    for (const char c : str)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' ||
            c == '_' || c == '~' || c == '^')
        {
            result += c;
        }
        else
        {
            result += '_';
        }
    }
    return result;
}

static std::string ToSpectatorId(const std::string& name, const std::unordered_map<std::string, std::string>& tags)
{
    std::string id = Sanitize(name);

    for (const auto& [key, val] : tags)
    {
        id += ',';
        id += Sanitize(key);
        id += '=';
        id += Sanitize(val);
    }

    return id;
}

MeterId MeterId::WithTag(const std::string& key, const std::string& value) const
{
    MeterId result;
    result.m_name = m_name;
    result.m_tags = m_tags;
    result.m_spectatord_id = m_spectatord_id;

    if (!IsEmptyOrWhitespace(key) && !IsEmptyOrWhitespace(value))
    {
        auto [it, inserted] = result.m_tags.emplace(key, value);
        if (inserted)
        {
            result.m_spectatord_id += ',';
            result.m_spectatord_id += Sanitize(key);
            result.m_spectatord_id += '=';
            result.m_spectatord_id += Sanitize(value);
        }
        else
        {
            it->second = value;
            result.m_spectatord_id = ToSpectatorId(result.m_name, result.m_tags);
        }
    }
    return result;
}

MeterId MeterId::WithTags(const std::unordered_map<std::string, std::string>& additional_tags) const
{
    MeterId result;
    result.m_name = m_name;
    result.m_tags = m_tags;
    result.m_spectatord_id = m_spectatord_id;

    bool needs_rebuild = false;
    for (const auto& [k, v] : ValidateTags(additional_tags))
    {
        auto [it, inserted] = result.m_tags.emplace(k, v);
        if (inserted)
        {
            result.m_spectatord_id += ',';
            result.m_spectatord_id += Sanitize(k);
            result.m_spectatord_id += '=';
            result.m_spectatord_id += Sanitize(v);
        }
        else
        {
            it->second = v;
            needs_rebuild = true;
        }
    }
    if (needs_rebuild)
    {
        result.m_spectatord_id = ToSpectatorId(result.m_name, result.m_tags);
    }
    return result;
}

MeterId::MeterId(const std::string& name, const std::unordered_map<std::string, std::string>& tags, const ExtraCommonTags* extra)
    : m_name(name), m_tags(ValidateTags(tags))
{
    m_spectatord_id = ToSpectatorId(m_name, m_tags);

    if (extra && !extra->tags.empty())
    {
        m_tags.insert(extra->tags.begin(), extra->tags.end());
        if (!extra->prefixString.empty())
        {
            m_spectatord_id += ',';
            m_spectatord_id += extra->prefixString;
        }
    }
}

std::string MeterId::to_string() const
{
    std::map<std::string, std::string> sorted(m_tags.begin(), m_tags.end());

    std::ostringstream ss;
    ss << "MeterId(name=" << m_name << ", tags={";
    bool first = true;
    for (const auto& [key, value] : sorted)
    {
        if (!first) ss << ", ";
        ss << "'" << key << "': '" << value << "'";
        first = false;
    }
    ss << "})";
    return ss.str();
}

}  // namespace spectator
