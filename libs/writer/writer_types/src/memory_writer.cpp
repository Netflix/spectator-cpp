#include <memory_writer.h>

#include <logger.h>

void MemoryWriter::Write(const std::string& message)
{
    this->m_messages.push_back(message);
}

void MemoryWriter::Close()
{
    this->Clear();
}

void MemoryWriter::Clear()
{
    this->m_messages.clear();
}

const std::string& MemoryWriter::LastLine() const noexcept
{
    static const std::string emptyString{};

    if (true == m_messages.empty())
    {
        return emptyString;
    }

    return this->m_messages.back();
}
