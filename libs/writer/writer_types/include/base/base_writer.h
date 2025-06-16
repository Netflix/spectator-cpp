#pragma once

#include <string>

class BaseWriter
{
   public:
    BaseWriter() = default;
    virtual ~BaseWriter() = default;

    BaseWriter(const BaseWriter&) = delete;
    BaseWriter& operator=(const BaseWriter&) = delete;
    BaseWriter(BaseWriter&&) = delete;
    BaseWriter& operator=(BaseWriter&&) = delete;

    virtual void Write(const std::string& message) = 0;
    virtual void Close() = 0;
};