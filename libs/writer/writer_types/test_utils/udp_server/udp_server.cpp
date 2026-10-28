#include "udp_server.h"

#include <boost/asio.hpp>
#include <poll.h>

#include <iostream>
#include <mutex>
#include <thread>

namespace {

std::vector<std::string> messages;
std::mutex messages_mutex;
std::atomic<bool> udp_server_ready{false};

// Larger than any datagram a writer sends, so nothing is truncated.
constexpr std::size_t kReceiveBufferSize = 64 * 1024;
// Room for bursts from many writer threads while this loop is busy.
constexpr int kSocketReceiveBuffer = 4 * 1024 * 1024;
// How often the loop checks udp_server_running while no data arrives.
constexpr int kPollTimeoutMs = 20;

}  // namespace

std::atomic<bool> udp_server_running(true);

std::vector<std::string> get_udp_messages()
{
    std::lock_guard<std::mutex> lock(messages_mutex);
    return messages;
}

void clear_udp_messages()
{
    std::lock_guard<std::mutex> lock(messages_mutex);
    messages.clear();
}

void add_message(const std::string& message)
{
    std::lock_guard<std::mutex> lock(messages_mutex);
    messages.push_back(message);
}

bool wait_for_udp_server(std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!udp_server_ready.load())
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

void listen_for_udp_messages(int port)
try
{
    boost::asio::io_context io_context;
    boost::asio::ip::udp::socket socket(io_context, boost::asio::ip::udp::v4());

    // A failure here only makes drops under load more likely, so keep going.
    boost::system::error_code ec;
    socket.set_option(boost::asio::socket_base::receive_buffer_size(kSocketReceiveBuffer), ec);

    socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port), ec);
    if (ec)
    {
        std::cerr << "UDP server: failed to bind 127.0.0.1:" << port << ": " << ec.message() << std::endl;
        return;
    }
    socket.non_blocking(true);
    udp_server_ready = true;

    std::vector<char> buffer(kReceiveBufferSize);
    while (udp_server_running)
    {
        pollfd pollFd{socket.native_handle(), POLLIN, 0};
        if (::poll(&pollFd, 1, kPollTimeoutMs) <= 0)
        {
            continue;
        }

        // Read everything that is queued before waiting again.
        for (;;)
        {
            boost::system::error_code read_ec;
            const std::size_t bytes_received = socket.receive(boost::asio::buffer(buffer), 0, read_ec);
            if (read_ec == boost::asio::error::would_block)
            {
                break;
            }
            if (read_ec)
            {
                std::cerr << "UDP server: receive failed: " << read_ec.message() << std::endl;
                break;
            }
            add_message(std::string(buffer.data(), bytes_received));
        }
    }

    udp_server_ready = false;
}
catch (const std::exception& e)
{
    udp_server_ready = false;
    std::cerr << "Exception in UDP server: " << e.what() << std::endl;
}

// Only compile the main function when building the executable, not the library
#ifndef UDP_SERVER_LIB_ONLY
int main()
{
    listen_for_udp_messages();
    return 0;
}
#endif
