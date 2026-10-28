#include <udp_writer.h>

#include "socket_options.h"

#include <logger.h>

namespace spectator {

namespace {
constexpr int kSendAttempts = 3;
}

UDPWriter::UDPWriter(const std::string& host, int port)
    : m_host(host),
      m_port(port),
      m_io_context(std::make_unique<boost::asio::io_context>()),
      m_socket(nullptr),
      m_socketEstablished(false),
      m_maxPayloadSize(detail::kMaxDatagramPayload)
{
    if (false == CreateSocket())
    {
        Logger::error("UDPWriter: Failed to create socket for {}:{} during construction", m_host, m_port);
    }
}

UDPWriter::~UDPWriter() { Close(); }

bool UDPWriter::CreateSocket()
try
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

    if (const auto timeoutError = detail::SetSendTimeout(m_socket->native_handle()); timeoutError)
    {
        Logger::warn("UDPWriter: Failed to set a send timeout - {}", timeoutError.message());
    }
    m_maxPayloadSize = detail::MaxDatagramPayload(m_socket->native_handle());

    m_endpoint = boost::asio::ip::udp::endpoint(boost::asio::ip::make_address(m_host), m_port);
    m_socketEstablished = true;
    Logger::info("UDPWriter: Socket created for {}:{}", m_host, m_port);
    return true;
}
catch (const boost::system::system_error& ex)
{
    Logger::error("UDP Writer: Boost exception: {}", ex.what());
    return false;
}

bool UDPWriter::TryToSend(std::string_view message)
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
    Logger::error("UDP Writer: Failed to send {} bytes to {}:{} - {}", message.size(), m_host, m_port, ec.message());
    return false;
}

void UDPWriter::Write(std::string_view message)
{
    if (false == this->m_socketEstablished && false == this->CreateSocket())
    {
        Logger::error("UDPWriter: Failed to write message, socket not established {}:{}", m_host, m_port);
        return;
    }

    if (false == TryToSend(message))
    {
        this->Close();
    }
}

void UDPWriter::Close()
try
{
    this->m_socketEstablished = false;
    if (m_socket && m_socket->is_open())
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

}  // namespace spectator
