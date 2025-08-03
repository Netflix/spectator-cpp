#pragma once

#include <base_writer.h>

#include <string>
#include <memory>

class UDSWriter final : public BaseWriter
{
   public:
    UDSWriter(const std::string& socketPath);
    ~UDSWriter() override;
    void Write(const std::string& message) override;
    void Close() override;

   private:
    // Forward declaration for implementation details
    class Impl;
    std::unique_ptr<Impl> m_pImpl;
};
