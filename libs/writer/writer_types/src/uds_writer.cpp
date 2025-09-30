#include <uds_writer.h>

#include <logger.h>
#include <boost/asio.hpp>

class UDSWriter::Impl
{
public:
    Impl(const std::string& socketPath);
    ~Impl() = default;
    
    bool CreateSocket();
    bool TryToSend(const std::string& message);
    void Close();
    
    std::string m_socketPath;
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::unique_ptr<boost::asio::local::datagram_protocol::socket> m_socket;
    boost::asio::local::datagram_protocol::endpoint m_endpoint;
    bool m_socketEstablished;
};

UDSWriter::Impl::Impl(const std::string& socketPath)
    : m_socketPath(socketPath),
      m_ioContext(std::make_unique<boost::asio::io_context>()),
      m_socket(nullptr),
      m_socketEstablished(false)
{
}

UDSWriter::UDSWriter(const std::string& socketPath)
    : m_pImpl(std::make_unique<Impl>(socketPath))
{
    if (false == m_pImpl->CreateSocket())
    {
        Logger::error("UDS Writer: Failed to create socket for {} during construction", m_pImpl->m_socketPath);
    }
}

UDSWriter::~UDSWriter() { Close(); }

bool UDSWriter::Impl::CreateSocket() try
{
    if (m_socketEstablished)
    {
        this->Close();
    }
    
    boost::system::error_code ec;
    m_socket = std::make_unique<boost::asio::local::datagram_protocol::socket>(*m_ioContext);
    m_socket->open(boost::asio::local::datagram_protocol(), ec);
    if (ec)
    {
        Logger::error("UDS Writer: Failed to create socket - {}", ec.message());
        return false;
    }

    m_endpoint = boost::asio::local::datagram_protocol::endpoint(m_socketPath);
    m_socketEstablished = true;
    Logger::info("UDS Writer: Socket created for {}", m_socketPath);
    return true;
}
catch (const boost::system::system_error& ex)
{
    Logger::error("UDS Writer: Boost exception: {}", ex.what());
    return false;
}

bool UDSWriter::Impl::TryToSend(const std::string& message) try
{
    boost::system::error_code ec;
    for (int i = 0; i < 3; i++)
    {
        size_t sent = m_socket->send_to(boost::asio::buffer(message), m_endpoint, 0, ec);
        if (ec || sent < message.size())
        {   
            Logger::error("UDS Writer: Failed to send message - {}, sent {} bytes out of {}", ec.message(), sent, message.size());
            continue;
        }
        return true;
    }
    return false;
}
catch (const boost::system::system_error& ex)
{
    Logger::error("UDS Writer: Boost exception: {}", ex.what());
    return false;
}

void UDSWriter::Write(const std::string& message)
{
    if (false == m_pImpl->m_socketEstablished && false == m_pImpl->CreateSocket())
    {
        Logger::error("UDS Writer: Failed to write message, socket not established {}", m_pImpl->m_socketPath);
        return;
    }

    if (false == m_pImpl->TryToSend(message))
    {
        Logger::error("UDS Writer: Failed to send message: {}", message);
        m_pImpl->Close();
    }
}

void UDSWriter::Impl::Close() try
{
    this->m_socketEstablished = false;
    if (m_socket != nullptr && m_socket->is_open())
    {
        boost::system::error_code ec;
        m_socket->close(ec);
        if (ec)
        {
            Logger::error("UDS Writer: Error closing existing socket - {}", ec.message());
        }
    }
}
catch (const boost::system::system_error& ex)
{
    Logger::error("UDS Writer: Boost exception: {}", ex.what());
}

void UDSWriter::Close()
{
    if (m_pImpl)
    {
        m_pImpl->Close();
    }
}
