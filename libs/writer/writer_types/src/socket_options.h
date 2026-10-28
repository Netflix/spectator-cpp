#pragma once

#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>

namespace spectator::detail {

// The largest UDP payload over IPv4. Unix datagram sockets on Linux accept more, but both transports stay at or below
// this so a buffer sized for one works for the other.
inline constexpr std::size_t kMaxDatagramPayload = 65507;

// How long one send may wait on a full receiver, such as a stalled spectatord, before it gives up.
inline constexpr std::chrono::milliseconds kSendTimeout{100};

// Asio's blocking send_to waits again, without limit, when the kernel times out a send. The writers therefore set
// SO_SNDTIMEO and call sendto themselves.
inline boost::system::error_code SetSendTimeout(int fd)
{
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>(std::chrono::microseconds(kSendTimeout).count());
    if (::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0)
    {
        return {errno, boost::system::system_category()};
    }
    return {};
}

// The largest datagram the socket can send. The send buffer is the platform limit on macOS (2048 bytes for Unix
// sockets and 9216 for UDP by default); on Linux it is far larger, so kMaxDatagramPayload applies.
inline std::size_t MaxDatagramPayload(int fd)
{
    int sendBuffer = 0;
    socklen_t length = sizeof(sendBuffer);
    if (::getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendBuffer, &length) != 0 || sendBuffer <= 0)
    {
        return kMaxDatagramPayload;
    }
    return std::min(kMaxDatagramPayload, static_cast<std::size_t>(sendBuffer));
}

// Sends the payload as one datagram. An empty error code means all of it was sent.
inline boost::system::error_code SendDatagram(int fd, std::string_view payload, const sockaddr* address,
                                              socklen_t addressLength)
{
    for (;;)
    {
        const auto sent = ::sendto(fd, payload.data(), payload.size(), 0, address, addressLength);
        if (sent >= 0)
        {
            if (static_cast<std::size_t>(sent) == payload.size())
            {
                return {};
            }
            return boost::asio::error::message_size;
        }
        if (errno != EINTR)
        {
            return {errno, boost::system::system_category()};
        }
    }
}

// True when the kernel gave up waiting for room at the receiver. Retrying right away would only wait again.
inline bool IsSendTimeout(const boost::system::error_code& ec)
{
    return ec == boost::system::errc::resource_unavailable_try_again ||
           ec == boost::system::errc::operation_would_block;
}

}  // namespace spectator::detail
