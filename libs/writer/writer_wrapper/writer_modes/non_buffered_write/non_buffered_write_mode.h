#pragma once

#include <write_mode.h>
#include <base_writer.h>

namespace spectator {

class NonBufferedWriteMode final : public WriteMode
{
   public:
    explicit NonBufferedWriteMode(BaseWriter& writer);

    void Write(const std::string& message) override;

   private:
    BaseWriter& m_writer;
};

}  // namespace spectator
