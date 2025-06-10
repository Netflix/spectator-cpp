#include "udp_server.h"
#include <boost/asio.hpp>
#include <iostream>
#include <array>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>

//---------- Message Storage Implementation ----------

// Vector to store all received messages
std::vector<std::string> messages = {};
std::mutex messages_mutex;

// Flag to control server shutdown
std::atomic<bool> server_running(true);

// Expose functions to interact with the messages vector
std::vector<std::string> get_messages()
{
    std::lock_guard<std::mutex> lock(messages_mutex);
    return messages;
}

void clear_messages()
{
    std::lock_guard<std::mutex> lock(messages_mutex);
    messages.clear();
}

void add_message(const std::string &message)
{
    std::lock_guard<std::mutex> lock(messages_mutex);
    messages.push_back(message);
}

//---------- UDP Server Implementation ----------

void listen_for_udp_messages()
try
{
    const unsigned short port = 1234;

    boost::asio::io_context io_context;

    // Create an IPv4 socket bound to localhost (127.0.0.1) and port 1234
    boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port);
    boost::asio::ip::udp::socket socket(io_context, boost::asio::ip::udp::v4());

    try
    {
        socket.bind(endpoint);
        std::cout << "Socket bound to IPv4 localhost (127.0.0.1):" << port << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error binding socket: " << e.what() << "\n";
        throw;
    }

    std::array<char, 2048> buffer;
    boost::asio::ip::udp::endpoint sender_endpoint;

    std::cout << "UDP server listening on 127.0.0.1:" << port << " (IPv4 localhost)\n";

    // Configure socket with a small timeout so we can check the run flag
    socket.non_blocking(true);

    while (server_running)
    {
        try
        {
            std::size_t bytes_received = socket.receive_from(boost::asio::buffer(buffer), sender_endpoint);

            if (bytes_received == buffer.size())
            {
                std::cout << "Warning: Received datagram might have been truncated (buffer full)" << std::endl;
            }

            // Create string from received data and store it in the messages vector
            std::string message(buffer.data(), bytes_received);

            // Add the message to our global message storage
            add_message(message);

            // Get the current count of messages
            auto current_messages = get_messages();

            std::cout << "Received from " << sender_endpoint << ": " << message << std::endl;
            std::cout << "Total messages stored: " << current_messages.size() << std::endl;
        }
        catch (const boost::system::system_error &e)
        {
            if (e.code() == boost::asio::error::would_block)
            {
                // No data available, just wait a bit and try again
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            else
            {
                std::cerr << "Error receiving data: " << e.what() << std::endl;
                break;
            }
        }
    }
    std::cout << "UDP server shutting down..." << std::endl;
}
catch (const std::exception &e)
{
    std::cerr << "Exception in UDP server: " << e.what() << "\n";
    return;
}

//---------- Main Function ----------

// Only compile the main function when building the executable, not the library
#ifndef UDP_SERVER_LIB_ONLY
int main()
{
    listen_for_udp_messages();
    return 0;
}
#endif
