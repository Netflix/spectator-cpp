#pragma once

#include <base_writer.h>

namespace spectator {

// Discards everything. Selected with the "none" writer type to turn metrics off.
class NoopWriter final : public BaseWriter
{
   public:
    NoopWriter() = default;
    ~NoopWriter() override = default;

    void Write(std::string_view) override {}
    void Close() override {}
};

}  // namespace spectator
