#include "uds_server.h"

#include <boost/asio.hpp>
#include <boost/asio/local/datagram_protocol.hpp>
#include <poll.h>

#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>

namespace {

std::vector<std::string> uds_messages;
std::mutex uds_messages_mutex;
std::atomic<bool> uds_server_ready{false};

// Larger than any datagram a writer sends, so nothing is truncated.
constexpr std::size_t kReceiveBufferSize = 64 * 1024;
// Room for bursts from many writer threads while this loop is busy.
constexpr int kSocketReceiveBuffer = 4 * 1024 * 1024;
// How often the loop checks uds_server_running while no data arrives.
constexpr int kPollTimeoutMs = 20;

}  // namespace

std::atomic<bool> uds_server_running(true);

std::vector<std::string> get_uds_messages()
{
    std::lock_guard<std::mutex> lock(uds_messages_mutex);
    return uds_messages;
}

void clear_uds_messages()
{
    std::lock_guard<std::mutex> lock(uds_messages_mutex);
    uds_messages.clear();
}

void add_uds_message(const std::string& message)
{
    std::lock_guard<std::mutex> lock(uds_messages_mutex);
    uds_messages.push_back(message);
}

bool wait_for_uds_server(std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!uds_server_ready.load())
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

void listen_for_uds_messages(const std::string& socketPath)
try
{
    std::filesystem::remove(socketPath);

    boost::asio::io_context io_context;
    boost::asio::local::datagram_protocol::socket socket(io_context);

    boost::system::error_code ec;
    socket.open(boost::asio::local::datagram_protocol(), ec);
    if (ec)
    {
        std::cerr << "UDS server: failed to open socket: " << ec.message() << std::endl;
        return;
    }

    // A failure here only makes drops under load more likely, so keep going.
    socket.set_option(boost::asio::socket_base::receive_buffer_size(kSocketReceiveBuffer), ec);

    socket.bind(boost::asio::local::datagram_protocol::endpoint(socketPath), ec);
    if (ec)
    {
        std::cerr << "UDS server: failed to bind " << socketPath << ": " << ec.message() << std::endl;
        return;
    }
    socket.non_blocking(true);
    uds_server_ready = true;

    std::vector<char> buffer(kReceiveBufferSize);
    while (uds_server_running)
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
            const std::size_t bytes_read = socket.receive(boost::asio::buffer(buffer), 0, read_ec);
            if (read_ec == boost::asio::error::would_block)
            {
                break;
            }
            if (read_ec)
            {
                std::cerr << "UDS server: receive failed: " << read_ec.message() << std::endl;
                break;
            }
            add_uds_message(std::string(buffer.data(), bytes_read));
        }
    }

    uds_server_ready = false;
    socket.close(ec);
    std::filesystem::remove(socketPath);
}
catch (const std::exception& e)
{
    uds_server_ready = false;
    std::cerr << "Exception in UDS server: " << e.what() << std::endl;
}

// Only compile the main function when building the executable, not the library
#ifndef UDS_SERVER_LIB_ONLY
int main()
{
    listen_for_uds_messages();
    return 0;
}
#endif
