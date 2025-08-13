#include <uds_writer.h>

#include <logger.h>

UDSWriter::UDSWriter(const std::string& socketPath)
    : m_socketPath(socketPath),
      m_ioContext(std::make_unique<boost::asio::io_context>()),
      m_socket(nullptr),
      m_socketEstablished(false)
{
    if (false == CreateSocket())
    {
        Logger::error("UDS Writer: Failed to create socket for {} during construction", m_socketPath);
    }
}

UDSWriter::~UDSWriter() { Close(); }

bool UDSWriter::CreateSocket()
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

void UDSWriter::Write(const std::string& message)
{
    if (false == this->m_socketEstablished && false == this->CreateSocket())
    {
        Logger::error("UDS Writer: Failed to write message, socket not established {}", m_socketPath);
        return;
    }

    boost::system::error_code ec;
    for (int i = 0; i < 3; i++)
    {
        size_t sent = m_socket->send_to(boost::asio::buffer(message), m_endpoint, 0, ec);
        if (ec || sent < message.size())
        {   
            Logger::error("UDS Writer: Failed to send message - {}, sent {} bytes out of {}", ec.message(), sent, message.size());
        }
        break;
    }    
}

void UDSWriter::Close()
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
