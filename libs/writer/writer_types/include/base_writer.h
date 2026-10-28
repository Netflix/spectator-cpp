#pragma once

#include <cstddef>
#include <limits>
#include <string_view>

namespace spectator {

// A transport for protocol lines. Each Write delivers its payload as one datagram. Implementations do not need to be
// thread-safe: the Writer sends through a Sink that serializes every call.
class BaseWriter
{
   public:
    BaseWriter() = default;
    virtual ~BaseWriter() = default;

    BaseWriter(const BaseWriter&) = delete;
    BaseWriter& operator=(const BaseWriter&) = delete;
    BaseWriter(BaseWriter&&) = delete;
    BaseWriter& operator=(BaseWriter&&) = delete;

    virtual void Write(std::string_view payload) = 0;
    virtual void Close() = 0;

    // The largest payload one Write can deliver. Per-thread buffers are clamped to it, so a full buffer always goes
    // out in a single Write. Transports without a datagram limit keep the default.
    [[nodiscard]] virtual std::size_t MaxPayloadSize() const noexcept
    {
        return std::numeric_limits<std::size_t>::max();
    }
};

}  // namespace spectator
