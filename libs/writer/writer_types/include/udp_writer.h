#pragma once

#include <base_writer.h>

#include <memory>
#include <string>
#include <boost/asio.hpp>

namespace spectator {

class UDPWriter final : public BaseWriter
{
   public:
    UDPWriter(const std::string& host, int port);
    ~UDPWriter() override;
    void Write(std::string_view message) override;
    void Close() override;
    [[nodiscard]] std::size_t MaxPayloadSize() const noexcept override { return m_maxPayloadSize; }

   private:
    std::string m_host;
    int m_port;
    std::unique_ptr<boost::asio::io_context> m_io_context;
    std::unique_ptr<boost::asio::ip::udp::socket> m_socket;
    boost::asio::ip::udp::endpoint m_endpoint;
    bool m_socketEstablished;
    std::size_t m_maxPayloadSize;

    bool CreateSocket();
    bool TryToSend(std::string_view message);
};

}  // namespace spectator
