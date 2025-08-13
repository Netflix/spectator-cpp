#pragma once

#include <base_writer.h>

#include <string>
#include <boost/asio.hpp>
#include <memory>

class UDSWriter final : public BaseWriter
{
   public:
    UDSWriter(const std::string& socketPath);
    ~UDSWriter() override;
    void Write(const std::string& message) override;
    void Close() override;

   private:
    std::string m_socketPath;
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::unique_ptr<boost::asio::local::datagram_protocol::socket> m_socket;
    boost::asio::local::datagram_protocol::endpoint m_endpoint;
    bool m_socketEstablished;
    
    bool CreateSocket();
};
