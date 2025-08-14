#pragma once

#include <base_writer.h>

#include <memory>
#include <string>
#include <boost/asio.hpp>

class UDPWriter final : public BaseWriter
{
   public:
    UDPWriter(const std::string& host, int port);
    ~UDPWriter() override;
    void Write(const std::string& message) override;
    void Close() override;

   private:
    std::string m_host;
    int m_port;
    std::unique_ptr<boost::asio::io_context> m_io_context;
    std::unique_ptr<boost::asio::ip::udp::socket> m_socket;
    boost::asio::ip::udp::endpoint m_endpoint;
    bool m_socketEstablished;
    
    bool CreateSocket();
    bool TryToSend(const std::string& message);
};
