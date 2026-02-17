#include "non_buffered_write_mode.h"

namespace spectator {

static constexpr auto NEW_LINE = '\n';

NonBufferedWriteMode::NonBufferedWriteMode(BaseWriter& writer) : m_writer(writer) {}

void NonBufferedWriteMode::Write(const std::string& message)
{
    m_writer.Write(message + NEW_LINE);
}

}  // namespace spectator
