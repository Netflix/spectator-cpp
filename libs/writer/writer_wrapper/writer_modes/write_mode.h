#pragma once

#include <string>

namespace spectator {

enum class WriteModeType
{
    Auto,              // bufferSize > 0 → Buffered, else NonBuffered
    NonBuffered,
    Buffered,
    LockFreeBuffered
};

class WriteMode
{
   public:
    WriteMode() = default;
    virtual ~WriteMode() = default;

    WriteMode(const WriteMode&) = delete;
    WriteMode& operator=(const WriteMode&) = delete;
    WriteMode(WriteMode&&) = delete;
    WriteMode& operator=(WriteMode&&) = delete;

    virtual void Write(const std::string& message) = 0;
};

}  // namespace spectator
