#include "uds_server.h"
#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <boost/asio/local/datagram_protocol.hpp>
#include <iostream>
#include <array>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>

//---------- Message Storage Implementation ----------

// Vector to store all received messages
std::vector<std::string> uds_messages = {};
std::mutex uds_messages_mutex;

// Flag to control server shutdown
std::atomic<bool> uds_server_running(true);

// Expose functions to interact with the messages vector
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

//---------- UDS Server Implementation ----------

void listen_for_uds_messages()
try
{
    // Path to the Unix domain socket
    const std::string socket_path = "/tmp/test_uds_socket";

    // Remove any existing socket file
    std::filesystem::remove(socket_path);

    boost::asio::io_context io_context;

    // Create and open a Unix domain datagram socket
    const boost::asio::local::datagram_protocol::endpoint endpoint(socket_path);
    boost::asio::local::datagram_protocol::socket socket(io_context);
    
    boost::system::error_code ec;
    socket.open(boost::asio::local::datagram_protocol(), ec);
    if (ec)
    {
        std::cerr << "Error opening socket: " << ec.message() << std::endl;
        return;
    }
    
    socket.bind(endpoint, ec);
    if (ec)
    {
        std::cerr << "Error binding to endpoint: " << ec.message() << std::endl;
        return;
    }

    // Set to non-blocking mode
    socket.non_blocking(true);

    std::cout << "UDS datagram server listening on " << socket_path << std::endl;

    // Buffer for reading data
    std::array<char, 2048> buffer{};
    while (uds_server_running)
    {
        try
        {
            // Client endpoint to receive the sender's address
            boost::asio::local::datagram_protocol::endpoint sender_endpoint;
            boost::system::error_code read_ec;
            std::size_t bytes_read = 0;
            
            // Try to receive a datagram
            bytes_read = socket.receive_from(boost::asio::buffer(buffer), sender_endpoint, 0, read_ec);
                
            // Handle the case where no data is available
            if (read_ec == boost::asio::error::would_block) {
                // No data available, wait a bit and try again
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            if (read_ec)
            {
                std::cerr << "Error reading from socket: " << read_ec.message() << std::endl;
                continue; // Continue to next iteration instead of breaking, as each datagram is independent
            }

            if (bytes_read == buffer.size())
            {
                std::cout << "Warning: Received data might have been truncated (buffer full)" << std::endl;
            }

            // Create string from received data and store it
            std::string message(buffer.data(), bytes_read);
            add_uds_message(message);
            auto current_messages = get_uds_messages();
            std::cout << "Received datagram: " << message << std::endl;
            std::cout << "Total messages stored: " << current_messages.size() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception in datagram server: " << e.what() << std::endl;
        }
    }

    std::cout << "UDS datagram server shutting down..." << std::endl;

    // Close the socket
    socket.close();

    // Clean up the socket file on exit
    std::filesystem::remove(socket_path);
}
catch (const std::exception& e)
{
    std::cerr << "Exception in UDS server: " << e.what() << std::endl;
    return;
}

//---------- Main Function ----------

// Only compile the main function when building the executable, not the library
#ifndef UDS_SERVER_LIB_ONLY
int main()
{
    listen_for_uds_messages();
    return 0;
}
#endif