#pragma once

#include <base_writer.h>

#include <string>
#include <boost/asio.hpp>
#include <memory>

namespace spectator {

class UDSWriter final : public BaseWriter
{
   public:
    explicit UDSWriter(const std::string& socketPath);
    ~UDSWriter() override;
    void Write(std::string_view message) override;
    void Close() override;
    [[nodiscard]] std::size_t MaxPayloadSize() const noexcept override { return m_maxPayloadSize; }

   private:
    std::string m_socketPath;
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::unique_ptr<boost::asio::local::datagram_protocol::socket> m_socket;
    boost::asio::local::datagram_protocol::endpoint m_endpoint;
    bool m_socketEstablished;
    std::size_t m_maxPayloadSize;

    bool CreateSocket();
    bool TryToSend(std::string_view message);
};

}  // namespace spectator
