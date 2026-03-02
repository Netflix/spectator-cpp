#pragma once

#include <string>

namespace spectator {

enum class WriterType
{
    Memory,
    UDP,
    Unix
};

class BaseWriter
{
   public:
    BaseWriter() = default;
    virtual ~BaseWriter() = default;

    BaseWriter(const BaseWriter&) = delete;
    BaseWriter& operator=(const BaseWriter&) = delete;
    BaseWriter(BaseWriter&&) = delete;
    BaseWriter& operator=(BaseWriter&&) = delete;

    virtual WriterType GetType() const = 0;
    virtual void Send(const std::string& message) = 0;
    virtual void Close() = 0;
};

}  // namespace spectator