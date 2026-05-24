#include <util.h>

#include <sstream>
#include <vector>

namespace spectator {

// Utility: split a string by a delimiter
std::vector<std::string> split(const std::string& str, const char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter))
    {
        tokens.push_back(item);
    }
    return tokens;
}

std::optional<ProtocolLine> ParseProtocolLine(const std::string& line)
{
    std::string symbol{};
    std::string name{};
    std::unordered_map<std::string, std::string> tags{};
    std::string value{};

    const auto mainParts = split(line, ':');

    if (mainParts.size() < 3)
    {
        return std::nullopt;
    }

    symbol = mainParts[0];

    auto idParts = split(mainParts[1], ',');
    if (!idParts.empty())
    {
        name = idParts[0];

        for (size_t i = 1; i < idParts.size(); ++i)
        {
            auto tagParts = split(idParts[i], '=');
            if (tagParts.size() == 2)
            {
                tags[tagParts[0]] = tagParts[1];
            }
        }
    }

    // The last part is the value
    value = mainParts[2];
    return ProtocolLine{symbol, MeterId{name, tags}, value};
}

bool IsEmptyOrWhitespace(const std::string& str)
{
    return str.empty() || std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isspace(c); });
}

std::unordered_map<std::string, std::string> ValidateTags(const std::unordered_map<std::string, std::string>& tags)
{
    std::unordered_map<std::string, std::string> valid_tags;
    for (const auto& tag : tags)
    {
        if (IsEmptyOrWhitespace(tag.first) == false && IsEmptyOrWhitespace(tag.second) == false)
        {
            valid_tags[tag.first] = tag.second;
        }
    }
    return valid_tags;
}

}  // namespace spectator