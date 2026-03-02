#pragma once

#include <write_mode.h>

namespace spectator {

class NonBufferedWriteMode final : public WriteMode
{
   public:
    explicit NonBufferedWriteMode(WriterType type, const std::string& param = "", int port = 0);

    WriteModeType GetType() const override { return WriteModeType::NonBuffered; }
    void Write(const std::string& message) override;
};

}  // namespace spectator
