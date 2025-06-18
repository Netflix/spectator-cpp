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
            m_socket = std::make_unique<local::stream_protocol::socket>(*m_ioContext);
        }

        // Connect to the UDS server
        const local::stream_protocol::endpoint endpoint(m_socketPath);

        boost::system::error_code ec;
        m_socket->connect(endpoint, ec);

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
        boost::asio::write(*m_socket, boost::asio::buffer(message), ec);

        if (ec)
        {
            Logger::error("UDS Writer: Failed to send message - {}", ec.message());
            m_isOpen = false;  // Mark as disconnected on error
        }
        else
        {
            Logger::debug("UDS Writer: Sent message ({} bytes)", message.size());
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
            m_socket->shutdown(local::stream_protocol::socket::shutdown_both, ec);
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
