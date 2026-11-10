#include <uds_writer.h>

#include <logger.h>

#include <algorithm>
#include <chrono>
#include <thread>

namespace spectator {

UDSWriter::UDSWriter(const std::string& socketPath, std::chrono::milliseconds receiverFullWait)
    : m_socketPath(socketPath),
      m_ioContext(std::make_unique<boost::asio::io_context>()),
      m_socket(nullptr),
      m_socketEstablished(false),
      m_receiverFullWait(receiverFullWait)
{
    if (false == CreateSocket())
    {
        Logger::error("UDS Writer: Failed to create socket for {} during construction", m_socketPath);
    }
}

UDSWriter::~UDSWriter() { Close(); }

bool UDSWriter::CreateSocket() try
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

bool UDSWriter::TryToSend(const std::string& message) try
{
    boost::system::error_code ec;
    // The time spent waiting for a receiver whose queue is full, at most m_receiverFullWait per message.
    auto waited = std::chrono::microseconds::zero();
    auto pause = std::chrono::microseconds(100);
    for (int i = 0; i < 3; i++)
    {
        size_t sent = m_socket->send_to(boost::asio::buffer(message), m_endpoint, 0, ec);
        // The receiver's queue is full. Give it time to read, as a blocking send does on Linux, instead of counting
        // this as a failed attempt. Never entered when m_receiverFullWait is zero.
        while (ec == boost::asio::error::no_buffer_space && waited < m_receiverFullWait)
        {
            std::this_thread::sleep_for(pause);
            waited += pause;
            pause = std::min(pause * 2, std::chrono::microseconds(5000));
            sent = m_socket->send_to(boost::asio::buffer(message), m_endpoint, 0, ec);
        }
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
    if (false == this->m_socketEstablished && false == this->CreateSocket())
    {
        Logger::error("UDS Writer: Failed to write message, socket not established {}", m_socketPath);
        return;
    }

    if (false == this->TryToSend(message))
    {
        Logger::error("UDS Writer: Failed to send message: {}", message);
        this->Close();
    }
}

void UDSWriter::Close() try
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

}  // namespace spectator
