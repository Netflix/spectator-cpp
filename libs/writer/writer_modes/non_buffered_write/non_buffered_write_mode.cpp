#include "non_buffered_write_mode.h"

namespace spectator {

static constexpr auto NEW_LINE = '\n';

NonBufferedWriteMode::NonBufferedWriteMode(std::unique_ptr<BaseWriter> writer)
    : WriteMode(std::move(writer)) {}

void NonBufferedWriteMode::Write(const std::string& message)
{
    m_writer->Send(message + NEW_LINE);
}

}  // namespace spectator
