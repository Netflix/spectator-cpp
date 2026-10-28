#include <thread_buffer.h>

namespace spectator::detail {

ThreadBuffer::ThreadBuffer(std::shared_ptr<Sink> sink, std::size_t capacity, bool dropOnFull,
                           std::function<void()> onDrop)
    : m_sink(std::move(sink)), m_capacity(capacity), m_dropOnFull(dropOnFull), m_onDrop(std::move(onDrop))
{
}

ThreadBuffer::AppendResult ThreadBuffer::Append(std::string_view line)
{
    const std::size_t bytesNeeded = line.size() + 1;  // the line and its newline

    std::unique_lock<std::mutex> data(m_dataMutex);
    if (m_closed.load(std::memory_order_relaxed))
    {
        return AppendResult::Closed;
    }

    if (m_pending.size() + bytesNeeded <= m_capacity)
    {
        AddToPending(line);
        return AppendResult::Appended;
    }

    if (bytesNeeded > m_capacity)
    {
        SendOversizedLine(line);
        return AppendResult::Appended;
    }

    if (m_dropOnFull)
    {
        data.unlock();
        if (m_onDrop)
        {
            m_onDrop();
        }
        return AppendResult::Dropped;
    }

    SendPendingThenAdd(line);
    return AppendResult::Appended;
}

void ThreadBuffer::SendPending(std::string& scratch)
{
    std::unique_lock<std::mutex> data(m_dataMutex);
    if (m_closed.load(std::memory_order_relaxed) || m_pending.empty())
    {
        return;
    }
    scratch.assign(m_pending);
    m_pending.clear();

    // Take sendMutex before releasing dataMutex, so a later flush by the owning thread can't overtake this content.
    std::lock_guard<std::mutex> send(m_sendMutex);
    data.unlock();
    m_sink->Send(scratch);
}

void ThreadBuffer::FlushAndClose()
{
    std::lock_guard<std::mutex> data(m_dataMutex);
    if (m_closed.load(std::memory_order_relaxed))
    {
        return;
    }
    {
        std::lock_guard<std::mutex> send(m_sendMutex);
        SendAndClearPending();
    }
    std::string().swap(m_pending);
    m_closed.store(true, std::memory_order_release);
}

void ThreadBuffer::AddToPending(std::string_view line)
{
    m_pending.append(line);
    m_pending.push_back('\n');
}

void ThreadBuffer::SendOversizedLine(std::string_view line)
{
    std::lock_guard<std::mutex> send(m_sendMutex);
    SendAndClearPending();

    std::string payload;
    payload.reserve(line.size() + 1);
    payload.append(line);
    payload.push_back('\n');
    m_sink->Send(payload);
}

void ThreadBuffer::SendPendingThenAdd(std::string_view line)
{
    std::lock_guard<std::mutex> send(m_sendMutex);
    SendAndClearPending();
    AddToPending(line);
}

void ThreadBuffer::SendAndClearPending()
{
    if (!m_pending.empty())
    {
        m_sink->Send(m_pending);
        m_pending.clear();
    }
}

}  // namespace spectator::detail
