#pragma once

#include <base_writer.h>

#include <memory>
#include <string>

class UDPWriter final : public BaseWriter
{
   public:
    UDPWriter(const std::string& host, int port);
    ~UDPWriter() override;
    void Write(const std::string& message) override;
    void Close() override;

   private:
    class Impl;
    std::unique_ptr<Impl> m_pImpl;
};
