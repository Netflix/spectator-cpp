#include <memory_writer.h>

namespace spectator {

void MemoryWriter::Write(std::string_view message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_messages.emplace_back(message);
}

void MemoryWriter::Close() { Clear(); }

void MemoryWriter::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_messages.clear();
}

std::vector<std::string> MemoryWriter::GetMessages() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_messages;
}

std::string MemoryWriter::LastLine() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_messages.empty() ? std::string() : m_messages.back();
}

bool MemoryWriter::IsEmpty() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_messages.empty();
}

}  // namespace spectator
