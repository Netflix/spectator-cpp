#include <uds_writer.h>

#include "socket_options.h"

#include <logger.h>

namespace spectator {

namespace {
constexpr int kSendAttempts = 3;
}

UDSWriter::UDSWriter(const std::string& socketPath)
    : m_socketPath(socketPath),
      m_ioContext(std::make_unique<boost::asio::io_context>()),
      m_socket(nullptr),
      m_socketEstablished(false),
      m_maxPayloadSize(detail::kMaxDatagramPayload)
{
    if (false == CreateSocket())
    {
        Logger::error("UDS Writer: Failed to create socket for {} during construction", m_socketPath);
    }
}

UDSWriter::~UDSWriter() { Close(); }

bool UDSWriter::CreateSocket()
try
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

    if (const auto timeoutError = detail::SetSendTimeout(m_socket->native_handle()); timeoutError)
    {
        Logger::warn("UDS Writer: Failed to set a send timeout - {}", timeoutError.message());
    }
    m_maxPayloadSize = detail::MaxDatagramPayload(m_socket->native_handle());

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

bool UDSWriter::TryToSend(std::string_view message)
{
    boost::system::error_code ec;
    for (int attempt = 0; attempt < kSendAttempts; ++attempt)
    {
        ec = detail::SendDatagram(m_socket->native_handle(), message, m_endpoint.data(),
                                  static_cast<socklen_t>(m_endpoint.size()));
        if (!ec)
        {
            return true;
        }
        if (detail::IsSendTimeout(ec))
        {
            break;
        }
    }
    Logger::error("UDS Writer: Failed to send {} bytes to {} - {}", message.size(), m_socketPath, ec.message());
    return false;
}

void UDSWriter::Write(std::string_view message)
{
    if (false == this->m_socketEstablished && false == this->CreateSocket())
    {
        Logger::error("UDS Writer: Failed to write message, socket not established {}", m_socketPath);
        return;
    }

    if (false == this->TryToSend(message))
    {
        this->Close();
    }
}

void UDSWriter::Close()
try
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
