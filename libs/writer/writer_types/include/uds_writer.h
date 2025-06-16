#pragma once

#include <libs/writer/writer_types/include/base_writer.h>

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
    std::unique_ptr<boost::asio::local::stream_protocol::socket> m_socket;
    bool m_isOpen;

    // Helper method to initialize the connection
    bool connect();
};
