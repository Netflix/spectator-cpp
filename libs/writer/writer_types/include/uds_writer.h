#pragma once

#include <base_writer.h>

#include <chrono>
#include <string>
#include <boost/asio.hpp>
#include <memory>

namespace spectator {

class UDSWriter final : public BaseWriter
{
   public:
    // receiverFullWait: how long Write() may wait for the receiver to make room when its queue is full, before the
    // message is given up as a failed send. Linux makes the sender wait in that case; macOS fails the send with
    // "no buffer space" instead, and Write() then retries for this long. With the default of zero such a send fails
    // at once, like any other error.
    UDSWriter(const std::string& socketPath,
              std::chrono::milliseconds receiverFullWait = std::chrono::milliseconds::zero());
    ~UDSWriter() override;
    void Write(const std::string& message) override;
    void Close() override;

   private:
    std::string m_socketPath;
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::unique_ptr<boost::asio::local::datagram_protocol::socket> m_socket;
    boost::asio::local::datagram_protocol::endpoint m_endpoint;
    bool m_socketEstablished;
    std::chrono::milliseconds m_receiverFullWait;
    
    bool CreateSocket();
    bool TryToSend(const std::string& message);
};

}  // namespace spectator
