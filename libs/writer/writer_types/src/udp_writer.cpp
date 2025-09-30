#include <udp_writer.h>

#include <logger.h>
#include <boost/asio.hpp>

class UDPWriter::Impl
{
public:
    Impl(const std::string& host, int port);
    ~Impl() = default;
    
    bool CreateSocket();
    bool TryToSend(const std::string& message);
    void Close();
    
    std::string m_host;
    int m_port;
    std::unique_ptr<boost::asio::io_context> m_io_context;
    std::unique_ptr<boost::asio::ip::udp::socket> m_socket;
    boost::asio::ip::udp::endpoint m_endpoint;
    bool m_socketEstablished;
};

UDPWriter::Impl::Impl(const std::string& host, int port) : 
    m_host(host), 
    m_port(port),
    m_io_context(std::make_unique<boost::asio::io_context>()),
    m_socket(nullptr), 
    m_socketEstablished(false)
{
}

UDPWriter::UDPWriter(const std::string& host, int port) : 
    m_pImpl(std::make_unique<Impl>(host, port))
{
    if (false == m_pImpl->CreateSocket())
    {
        Logger::error("UDPWriter: Failed to create socket for {}:{} during construction", m_pImpl->m_host, m_pImpl->m_port);
    }
}

UDPWriter::~UDPWriter() { Close(); }

bool UDPWriter::Impl::CreateSocket() try
{
    if (m_socketEstablished)
    {
        this->Close();
    }

    boost::system::error_code ec;
    m_socket = std::make_unique<boost::asio::ip::udp::socket>(*m_io_context);
    m_socket->open(boost::asio::ip::udp::v4(), ec);
    if (ec)
    {
        Logger::error("UDPWriter: Failed to create socket - {}", ec.message());
        return false;
    }
    
    m_endpoint = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(m_host), m_port);
    m_socketEstablished = true;
    Logger::info("UDPWriter: Socket created for {}:{}", m_host, m_port);
    return true;
}
catch (const boost::system::system_error& ex)
{
    Logger::error("UDP Writer: Boost exception: {}", ex.what());
    return false;
}

bool UDPWriter::Impl::TryToSend(const std::string& message) try
{
    boost::system::error_code ec;
    for (int i = 0; i < 3; i++)
    {
        size_t sent = m_socket->send_to(boost::asio::buffer(message.data(), message.size()), m_endpoint, 0, ec);
        if (ec || sent < message.size())
        {   
            Logger::error("UDP Writer: Failed to send message - {}, sent {} bytes out of {}", ec.message(), sent, message.size());
            continue;
        }
        return true;
    }
    return false;
}
catch (const boost::system::system_error& ex)
{
    Logger::error("UDP Writer: Boost exception: {}", ex.what());
    return false;
}

void UDPWriter::Write(const std::string& message)
{
    if (false == m_pImpl->m_socketEstablished && false == m_pImpl->CreateSocket())
    {
        Logger::error("UDPWriter: Failed to write message, socket not established {}:{}", m_pImpl->m_host, m_pImpl->m_port);
        return;
    }

    if (m_pImpl->TryToSend(message) == false)
    {
        Logger::error("UDP Writer: Failed to send message: {}", message);
        m_pImpl->Close();
    }
}

void UDPWriter::Impl::Close() try
{
    this->m_socketEstablished = false;
    if(m_socket && m_socket->is_open())
    {
        boost::system::error_code ec;
        m_socket->close(ec);
        if (ec)
        {
            Logger::error("UDP Writer: Error closing existing socket - {}", ec.message());
        }
    }
}
catch (const boost::system::system_error& ex)
{
    Logger::error("UDP Writer: Boost exception: {}", ex.what());
}

void UDPWriter::Close()
{
    if (m_pImpl)
    {
        m_pImpl->Close();
    }
}