#include <libs/writer/writer_types/include/uds_writer.h>

#include <libs/logger/logger.h>

#include <boost/asio.hpp>

namespace local = boost::asio::local;

UDSWriter::UDSWriter(const std::string& socketPath)
    : m_socketPath(socketPath),
      m_ioContext(std::make_unique<boost::asio::io_context>()),
      m_socket(nullptr),
      m_isOpen(false)
{
    connect();
}

UDSWriter::~UDSWriter() { Close(); }

bool UDSWriter::connect()
{
    if (m_isOpen)
    {
        return true;  // Already connected
    }

    try
    {
        // Create a new socket if needed
        if (!m_socket)
        {
            m_socket = std::make_unique<local::datagram_protocol::socket>(*m_ioContext);
        }

        // Open the socket and prepare the endpoint
        boost::system::error_code ec;
        m_socket->open(local::datagram_protocol(), ec);
        
        // Set up the server endpoint
        m_endpoint = local::datagram_protocol::endpoint(m_socketPath);

        if (ec)
        {
            Logger::error("UDS Writer: Failed to connect to {} - {}", m_socketPath, ec.message());
            return false;
        }

        m_isOpen = true;
        Logger::debug("UDS Writer: Connected to {}", m_socketPath);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::error("UDS Writer: Exception while connecting - {}", e.what());
        m_isOpen = false;
        return false;
    }
}

void UDSWriter::Write(const std::string& message)
{
    if (!m_isOpen && !connect())
    {
        Logger::error("UDS Writer: Cannot write - not connected to {}", m_socketPath);
        return;
    }

    try
    {
        boost::system::error_code ec;
        size_t sent = m_socket->send_to(boost::asio::buffer(message), m_endpoint, 0, ec);

        if (ec)
        {
            Logger::error("UDS Writer: Failed to send message - {}", ec.message());
            m_isOpen = false;  // Mark as disconnected on error
        }
        else
        {
            Logger::debug("UDS Writer: Sent message ({} bytes)", sent);
        }
    }
    catch (const std::exception& e)
    {
        Logger::error("UDS Writer: Exception while sending message - {}", e.what());
        m_isOpen = false;  // Mark as disconnected on exception
    }
}

void UDSWriter::Close()
{
    if (m_socket && m_isOpen)
    {
        try
        {
            boost::system::error_code ec;
            m_socket->close(ec);

            if (ec)
            {
                Logger::warn("UDS Writer: Error closing socket - {}", ec.message());
            }
        }
        catch (const std::exception& e)
        {
            Logger::warn("UDS Writer: Exception while closing socket - {}", e.what());
        }
    }

    m_isOpen = false;
    Logger::debug("UDS Writer: Connection closed");
}
