#pragma once

#include <write_mode.h>

namespace spectator {

class NonBufferedWriteMode final : public WriteMode
{
   public:
    explicit NonBufferedWriteMode(std::unique_ptr<BaseWriter> writer);

    WriteModeType GetType() const override { return WriteModeType::NonBuffered; }
    void Write(const std::string& message) override;
};

}  // namespace spectator
