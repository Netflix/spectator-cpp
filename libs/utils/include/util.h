# pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <sstream>
#include <map>

#include <libs/meter/meter_id/meter_id.h>

struct ProtocolLine
{
    char symbol;
    MeterId id;
    std::string value;

    bool operator==(const ProtocolLine& other) const
    {
        return symbol == other.symbol && id == other.id && value == other.value;
    }

    [[nodiscard]] std::string to_string() const
    {
        std::stringstream ss;
        ss << symbol << ":" << id.GetName();

        // Sort tags by key
        std::map<std::string, std::string> sorted_tags(id.GetTags().begin(), id.GetTags().end());

        // Add tags if there are any
        if (!sorted_tags.empty())
        {
            ss << ",";
            bool first = true;
            for (const auto& [key, value] : sorted_tags)
            {
                if (!first)
                {
                    ss << ",";
                }
                ss << key << "=" << value;
                first = false;
            }
        }

        // Add the value
        ss << ":" << value;

        return ss.str();
    }
};

std::optional<ProtocolLine> ParseProtocolLine(const std::string& line);

bool IsEmptyOrWhitespace(const std::string& str);