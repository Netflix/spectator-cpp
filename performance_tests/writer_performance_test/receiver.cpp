#include "receiver.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <system_error>
#include <vector>

namespace perf {

namespace {

// The receive buffer that is asked for, so that the receiver is not what limits the writer. The system may give
// less.
constexpr int kReceiveBufferBytes = 8 * 1024 * 1024;

// Larger than any datagram.
constexpr std::size_t kLargestDatagram = 1024 * 1024;

// Once the sender has gone, the receiver stops when the socket has stayed empty for this long.
constexpr int kQuietMillis = 50;

std::system_error Failure(const std::string& what)
{
    return std::system_error(errno, std::generic_category(), what);
}

// A new datagram socket, which the recording process does not inherit.
int OpenSocket(int family)
{
    const int descriptor = ::socket(family, SOCK_DGRAM, 0);
    if (descriptor < 0)
    {
        throw Failure("Could not create a socket");
    }
    ::fcntl(descriptor, F_SETFD, FD_CLOEXEC);
    return descriptor;
}

sockaddr_un UnixAddress(const std::string& path)
{
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
    return address;
}

// A receiving socket on this machine.
struct Bound
{
    int socket = -1;
    std::string location;
    std::string socketPath;
    sockaddr_in udpAddress{};
};

Bound Bind(WriterKind kind)
{
    Bound bound;
    if (kind == WriterKind::Udp)
    {
        bound.socket = OpenSocket(AF_INET);
        bound.udpAddress.sin_family = AF_INET;
        bound.udpAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bound.udpAddress.sin_port = 0;  // any port that is free
        socklen_t length = sizeof(bound.udpAddress);
        if (::bind(bound.socket, reinterpret_cast<const sockaddr*>(&bound.udpAddress), length) != 0 ||
            ::getsockname(bound.socket, reinterpret_cast<sockaddr*>(&bound.udpAddress), &length) != 0)
        {
            const std::system_error failure = Failure("Could not bind a UDP socket on 127.0.0.1");
            ::close(bound.socket);
            throw failure;
        }
        bound.location = "udp://127.0.0.1:" + std::to_string(ntohs(bound.udpAddress.sin_port));
    }
    else
    {
        static std::atomic<int> sockets{0};
        bound.socket = OpenSocket(AF_UNIX);
        bound.socketPath = "/tmp/spectator_perf_" + std::to_string(::getpid()) + "_" +
                           std::to_string(sockets.fetch_add(1)) + ".sock";
        ::unlink(bound.socketPath.c_str());
        const sockaddr_un address = UnixAddress(bound.socketPath);
        if (::bind(bound.socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
        {
            const std::system_error failure = Failure("Could not bind a Unix socket at " + bound.socketPath);
            ::close(bound.socket);
            throw failure;
        }
        bound.location = "unix://" + bound.socketPath;
    }

    const int size = kReceiveBufferBytes;
    ::setsockopt(bound.socket, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    return bound;
}

void Release(const Bound& bound)
{
    ::close(bound.socket);
    if (!bound.socketPath.empty())
    {
        ::unlink(bound.socketPath.c_str());
    }
}

// The same clock, read the same way, as the recording threads use for the value of a line.
std::uint64_t MicrosNow()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

}  // namespace

Receiver::Receiver(WriterKind kind)
{
    const Bound bound = Bind(kind);
    m_socket = bound.socket;
    m_location = bound.location;
    m_socketPath = bound.socketPath;
}

Receiver::~Receiver()
{
    Stop();
    ::close(m_socket);
    if (!m_socketPath.empty())
    {
        ::unlink(m_socketPath.c_str());
    }
}

void Receiver::Start(bool timestamps)
{
    m_timestamps = timestamps;
    m_thread = std::thread(&Receiver::Read, this);
}

Received Receiver::Stop()
{
    m_stopping.store(true);
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    return m_received;
}

void Receiver::Read()
{
    std::vector<char> datagram(kLargestDatagram);
    pollfd waiting{m_socket, POLLIN, 0};
    for (;;)
    {
        // Looked at before the socket is read, so that nothing that arrived before Stop() is left behind.
        const bool stopping = m_stopping.load();
        const ssize_t size = ::recv(m_socket, datagram.data(), datagram.size(), MSG_DONTWAIT);
        if (size < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                return;
            }
            // The socket is empty. While the sender runs, more will come. Once it has gone, what it sent last may
            // still take a moment to get here.
            const int ready = ::poll(&waiting, 1, stopping ? kQuietMillis : 20);
            if (ready == 0 && stopping)
            {
                return;
            }
            continue;
        }

        const auto bytes = static_cast<std::size_t>(size);
        ++m_received.datagrams;
        m_received.bytes += bytes;
        m_received.lines += static_cast<std::uint64_t>(std::count(datagram.data(), datagram.data() + bytes, '\n'));
        if (m_timestamps)
        {
            KeepDelays(datagram.data(), bytes);
        }
    }
}

void Receiver::KeepDelays(const char* datagram, std::size_t size)
{
    const std::uint64_t now = MicrosNow();
    const char* const end = datagram + size;
    for (const char* line = datagram; line < end;)
    {
        const void* const newline = std::memchr(line, '\n', static_cast<std::size_t>(end - line));
        const char* const lineEnd = newline != nullptr ? static_cast<const char*>(newline) : end;

        // A line is "c:<id>:<value>", and the whole part of the value is the time of recording in microseconds.
        const char* value = lineEnd;
        while (value > line && value[-1] != ':')
        {
            --value;
        }
        std::uint64_t recordedAt = 0;
        for (; value < lineEnd && *value >= '0' && *value <= '9'; ++value)
        {
            recordedAt = recordedAt * 10 + static_cast<std::uint64_t>(*value - '0');
        }
        m_received.delayMicros.Record(now > recordedAt ? now - recordedAt : 0);

        line = lineEnd + 1;
    }
}

bool Receiver::Carries(WriterKind kind, std::size_t bytes)
{
    const Bound receiver = Bind(kind);
    const std::vector<char> datagram(bytes, 'x');
    ssize_t sent = -1;
    try
    {
        // A socket as the library opens it, with the system's default limits.
        const int sender = OpenSocket(kind == WriterKind::Udp ? AF_INET : AF_UNIX);
        if (kind == WriterKind::Udp)
        {
            sent = ::sendto(sender, datagram.data(), datagram.size(), 0,
                            reinterpret_cast<const sockaddr*>(&receiver.udpAddress), sizeof(receiver.udpAddress));
        }
        else
        {
            const sockaddr_un address = UnixAddress(receiver.socketPath);
            sent = ::sendto(sender, datagram.data(), datagram.size(), 0, reinterpret_cast<const sockaddr*>(&address),
                            sizeof(address));
        }
        ::close(sender);
    }
    catch (const std::system_error&)
    {
        Release(receiver);
        throw;
    }
    Release(receiver);
    return sent == static_cast<ssize_t>(bytes);
}

}  // namespace perf
