#pragma once

#include "memory_writer.h"
#include "udp_writer.h"
#include "uds_writer.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_set>

namespace spectator {

// Enum to specify which writer type to create
enum class WriterType
{
    Memory,
    UDP,
    Unix
};

struct WriterTypes
{
    static constexpr auto Memory = "memory";
    static constexpr auto UDP = "udp";
    static constexpr auto Unix = "unix";

    // URL prefixes
    static constexpr auto UDPURL = "udp://";
    static constexpr auto UnixURL = "unix://";
};

struct DefaultLocations
{
    static constexpr auto NoLocation = "";
    static constexpr auto UDP = "udp://127.0.0.1:1234";
    static constexpr auto UDS = "unix:///run/spectatord/spectatord.unix";
};

inline const std::map<std::string_view, std::pair<WriterType, std::string_view>> TypeToLocationMap = {
    {WriterTypes::Memory, {WriterType::Memory, DefaultLocations::NoLocation}},
    {WriterTypes::UDP, {WriterType::UDP, DefaultLocations::UDP}},
    {WriterTypes::Unix, {WriterType::Unix, DefaultLocations::UDS}},
};

inline std::string WriterTypeToString(WriterType type)
{
    switch (type)
    {
        case WriterType::Memory:
            return WriterTypes::Memory;
        case WriterType::UDP:
            return WriterTypes::UDP;
        case WriterType::Unix:
            return WriterTypes::Unix;
        default:
            return "Unknown";
    }
}

}  // namespace spectator