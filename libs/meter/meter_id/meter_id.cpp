#include <meter_id.h>

#include <util.h>

#include <regex>
#include <sstream>

namespace spectator {

const std::regex INVALID_CHARS("[^-._A-Za-z0-9~^]");

static std::string Sanitize(const std::string& str)
{
    return std::regex_replace(str, INVALID_CHARS, "_");
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
    if (!IsEmptyOrWhitespace(key) && !IsEmptyOrWhitespace(value))
    {
        result.m_tags[key] = value;
    }
    result.m_spectatord_id = ToSpectatorId(result.m_name, result.m_tags);
    return result;
}

MeterId MeterId::WithTags(const std::unordered_map<std::string, std::string>& additional_tags) const
{
    MeterId result;
    result.m_name = m_name;
    result.m_tags = m_tags;
    const auto validated = ValidateTags(additional_tags);
    result.m_tags.insert(validated.begin(), validated.end());
    result.m_spectatord_id = ToSpectatorId(result.m_name, result.m_tags);
    return result;
}

MeterId::MeterId(const std::string& name, const std::unordered_map<std::string, std::string>& tags, std::shared_ptr<const ExtraCommonTags> extra)
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
