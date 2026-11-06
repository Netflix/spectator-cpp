#pragma once

#include "memory_writer.h"
#include "udp_writer.h"
#include "uds_writer.h"

#include <chrono>
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

// With buffering enabled, each thread fills its own buffer and may have a fixed number of full buffers waiting for
// the sender thread. This is what a thread does when it records a line that does not fit in its buffer while that
// number of full buffers is already waiting.
enum class FullBufferPolicy
{
    // Wait until the sender thread has taken one of the thread's buffers. No line is lost.
    Block,
    // Drop the line being recorded and return at once. Dropped lines are counted, and the count is logged.
    Drop,
    // Send the thread's oldest waiting buffer from the recording thread, then carry on. No line is lost.
    Send
};

struct BufferingDefaults
{
    // The sender thread collects and sends what every thread has buffered at least this often, so a line never
    // waits for its thread to fill a buffer. Three seconds keep a line from a quiet thread at most about three
    // seconds behind, and cost at most one wake-up of the sender and one small datagram per recording thread every
    // three seconds.
    static constexpr std::chrono::milliseconds FlushInterval{std::chrono::seconds{3}};
    // The longest flush interval that a WriterConfig accepts. A longer one would hold the lines of a quiet thread
    // back for longer than any consumer of the metrics expects.
    static constexpr std::chrono::milliseconds MaxFlushInterval{std::chrono::seconds{30}};
    static constexpr FullBufferPolicy FullBuffer = FullBufferPolicy::Block;
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

inline std::string FullBufferPolicyToString(FullBufferPolicy policy)
{
    switch (policy)
    {
        case FullBufferPolicy::Block:
            return "block";
        case FullBufferPolicy::Drop:
            return "drop";
        case FullBufferPolicy::Send:
            return "send";
        default:
            return "Unknown";
    }
}

}  // namespace spectator