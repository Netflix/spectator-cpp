#include <libs/writer/writer_types/include/memory_writer.h>

#include <libs/logger/logger.h>

void MemoryWriter::Write(const std::string &message)
{
    this->m_messages.push_back(message);
    Logger::debug("MemoryWriter::Writing: {}", message);
}

void MemoryWriter::Close()
{
    this->Clear();
    Logger::debug("MemoryWriter::Closed");
}

void MemoryWriter::Clear()
{
    this->m_messages.clear();
    Logger::debug("MemoryWriter::Cleared messages");
}

const std::string &MemoryWriter::LastLine() const noexcept
{
    static const std::string emptyString = "";

    if (true == m_messages.empty())
    {
        return emptyString;
    }

    return this->m_messages.back();
}
