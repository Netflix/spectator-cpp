#include "non_buffered_write_mode.h"
#include <logger.h>

namespace spectator {

static constexpr auto NEW_LINE = '\n';

NonBufferedWriteMode::NonBufferedWriteMode(WriterType type, const std::string& param, int port)
    : WriteMode(type, param, port)
{
    Logger::info("WriteMode mode: NonBuffered");
}

void NonBufferedWriteMode::Write(const std::string& message)
{
    m_writer->Send(message + NEW_LINE);
}

}  // namespace spectator
