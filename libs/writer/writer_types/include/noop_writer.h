#pragma once

#include <base_writer.h>
#include <string>

namespace spectator {

class NoopWriter final : public BaseWriter
{
   public:
    NoopWriter() = default;
    ~NoopWriter() override = default;

    void Write(const std::string&) override {}
    void Close() override {}
};

}  // namespace spectator
