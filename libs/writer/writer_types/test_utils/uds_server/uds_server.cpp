#include "uds_server.h"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <iostream>
#include <array>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
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

void add_uds_message(const std::string &message)
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

    // Create and open a Unix domain socket
    boost::asio::local::stream_protocol::endpoint endpoint(socket_path);
    boost::asio::local::stream_protocol::acceptor acceptor(io_context, endpoint);

    std::cout << "UDS server listening on " << socket_path << std::endl;

    while (uds_server_running)
    {
        // Create a socket for the client connection
        boost::asio::local::stream_protocol::socket socket(io_context);

        try
        {
            // Set acceptor to non-blocking so we can check the run flag
            acceptor.non_blocking(true);

            boost::system::error_code ec;
            acceptor.accept(socket, ec);

            if (ec)
            {
                if (ec == boost::asio::error::would_block)
                {
                    // No connection available, wait a bit and try again
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue;
                }
                else
                {
                    std::cerr << "Error accepting connection: " << ec.message() << std::endl;
                    continue;
                }
            }

            // Connection accepted, set back to blocking for data reading
            socket.non_blocking(false);

            // Buffer for reading data
            std::array<char, 2048> buffer;
            while (true)
            {
                boost::system::error_code read_ec;
                std::size_t bytes_read = socket.read_some(boost::asio::buffer(buffer), read_ec);

                if (read_ec == boost::asio::error::eof)
                {
                    // Connection closed cleanly by peer
                    break;
                }
                else if (read_ec)
                {
                    std::cerr << "Error reading from socket: " << read_ec.message() << std::endl;
                    break;
                }

                if (bytes_read == buffer.size())
                {
                    std::cout << "Warning: Received data might have been truncated (buffer full)" << std::endl;
                }

                // Create string from received data and store it
                std::string message(buffer.data(), bytes_read);
                add_uds_message(message);
                auto current_messages = get_uds_messages();
                std::cout << "Received message: " << message << std::endl;
                std::cout << "Total messages stored: " << current_messages.size() << std::endl;
            }
            // Close the connection
            socket.close();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Exception handling client: " << e.what() << std::endl;
        }
    }

    std::cout << "UDS server shutting down..." << std::endl;

    // Clean up the socket file on exit
    std::filesystem::remove(socket_path);
}
catch (const std::exception &e)
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