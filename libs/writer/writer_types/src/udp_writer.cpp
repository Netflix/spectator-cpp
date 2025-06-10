#include <libs/writer/writer_types/include/udp_writer.h>

#include <libs/logger/logger.h>

#include <boost/system/error_code.hpp>

UDPWriter::UDPWriter(const std::string &host, int port) : m_host(host), m_port(port)
{
    try
    {
        // Create io_context
        m_io_context = std::make_unique<boost::asio::io_context>();

        // Create socket
        m_socket = std::make_unique<boost::asio::ip::udp::socket>(*m_io_context);
        m_socket->open(boost::asio::ip::udp::v4());

        // Resolve the endpoint
        boost::asio::ip::udp::resolver resolver(*m_io_context);
        m_endpoint = *resolver.resolve(boost::asio::ip::udp::v4(), m_host, std::to_string(m_port)).begin();
    }
    catch (const boost::system::system_error &e)
    {
        Logger::error("UDPWriter: Failed to initialize connection: {}", e.what());
        Close();
    }
}

UDPWriter::~UDPWriter()
{
    Close();
}

void UDPWriter::Write(const std::string &message)
try
{
    if (m_socket == nullptr || m_socket->is_open() == false)
    {
        Logger::error("UDPWriter: Socket not initialized or closed");
        return;
    }

    boost::system::error_code ec;
    size_t sent = m_socket->send_to(boost::asio::buffer(message.data(), message.size()), m_endpoint, 0, ec);

    if (ec)
    {
        Logger::error("UDPWriter: Failed to send message: {}", ec.message());
    }
    else if (sent != message.size())
    {
        Logger::warn("UDPWriter: Sent only {} bytes out of {} bytes", sent, message.size());
    }
}
catch (const std::exception &e)
{
    Logger::error("UDPWriter: Exception during write: {}", e.what());
}

void UDPWriter::Close()
try
{

    if (m_socket && m_socket->is_open())
    {
        boost::system::error_code ec;
        m_socket->close(ec);
        if (ec)
        {
            Logger::warn("UDPWriter: Error when closing socket: {}", ec.message());
        }
    }

    // Reset the unique_ptr to deallocate resources
    m_socket.reset();
    m_io_context.reset();
}
catch (const std::exception &e)
{
    Logger::error("UDPWriter: Exception during close: {}", e.what());
}