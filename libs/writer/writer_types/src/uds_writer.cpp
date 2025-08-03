#include <uds_writer.h>

#include <logger.h>
#include <boost/asio.hpp>

namespace local = boost::asio::local;

// Implementation class definition
class UDSWriter::Impl
{
public:
    std::string m_socketPath;
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::unique_ptr<boost::asio::local::datagram_protocol::socket> m_socket;
    boost::asio::local::datagram_protocol::endpoint m_endpoint;
    bool m_isOpen;

    explicit Impl(const std::string& socketPath)
        : m_socketPath(socketPath),
          m_ioContext(std::make_unique<boost::asio::io_context>()),
          m_socket(nullptr),
          m_isOpen(false)
    {
    }

    bool connect()
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
            return true;
        }
        catch (const std::exception& e)
        {
            Logger::error("UDS Writer: Exception while connecting - {}", e.what());
            m_isOpen = false;
            return false;
        }
    }
};

UDSWriter::UDSWriter(const std::string& socketPath)
    : m_pImpl(std::make_unique<Impl>(socketPath))
{
    m_pImpl->connect();
}
UDSWriter::~UDSWriter() { Close(); }

void UDSWriter::Write(const std::string& message)
{
    if (!m_pImpl->m_isOpen && !m_pImpl->connect())
    {
        Logger::error("UDS Writer: Cannot write - not connected to {}", m_pImpl->m_socketPath);
        return;
    }

    try
    {
        boost::system::error_code ec;
        size_t sent = m_pImpl->m_socket->send_to(boost::asio::buffer(message), m_pImpl->m_endpoint, 0, ec);

        if (ec)
        {
            Logger::error("UDS Writer: Failed to send message - {}", ec.message());
            m_pImpl->m_isOpen = false;  // Mark as disconnected on error
        }
        if (sent < message.size())
        {
            Logger::error("UDS Writer: Sent only {} bytes out of {} bytes", sent, message.size());
        }
    }
    catch (const std::exception& e)
    {
        Logger::error("UDS Writer: Exception while sending message - {}", e.what());
        m_pImpl->m_isOpen = false;  // Mark as disconnected on exception
    }
}

void UDSWriter::Close()
{
    if (m_pImpl->m_socket && m_pImpl->m_isOpen)
    {
        try
        {
            boost::system::error_code ec;
            m_pImpl->m_socket->close(ec);

            if (ec)
            {
                Logger::error("UDS Writer: Error closing socket - {}", ec.message());
            }
        }
        catch (const std::exception& e)
        {
            Logger::error("UDS Writer: Exception while closing socket - {}", e.what());
        }
    }
    m_pImpl->m_isOpen = false;
}
