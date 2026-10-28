#include "writer_state.h"

#include <logger.h>

#include <algorithm>
#include <exception>

namespace spectator::detail {

// One thread's pending lines. Two mutexes keep a thread's lines in the order they were written without making appends
// wait for I/O:
// - dataMutex guards `pending` and `closed`. It is held only for short work, except by the owning thread while it
//   sends its own full buffer, and by CloseBuffer.
// - sendMutex is taken, while dataMutex is still held, by whoever takes the pending content, and held until that
//   content is sent. Content taken later therefore can't be sent first.
struct WriterState::ThreadBuffer
{
    std::mutex dataMutex;
    std::mutex sendMutex;
    std::string pending;
    // Written under dataMutex. Read without it by the drain pass when it forgets closed buffers.
    std::atomic<bool> closed{false};
};

namespace {

// Keeps `now + interval` far from overflowing however large the configured interval is.
constexpr std::chrono::milliseconds kMaxFlushInterval = std::chrono::hours(24);

// A full buffer is sent as one datagram, so it can't be larger than the transport allows.
std::size_t ChooseCapacity(unsigned int requested, std::size_t transportLimit)
{
    if (requested > transportLimit)
    {
        Logger::warn("Writer: buffer size {} exceeds the largest datagram this transport can send; using {}", requested,
                     transportLimit);
        return transportLimit;
    }
    return requested;
}

}  // namespace

// ---- Construction and shutdown ----

WriterState::WriterState(std::unique_ptr<BaseWriter> transport, const BufferConfig& config)
    : m_transport(std::move(transport)),
      m_capacity(ChooseCapacity(config.size, m_transport->MaxPayloadSize())),
      m_dropOnFull(config.dropOnFull),
      m_flushInterval(std::clamp(config.flushInterval, std::chrono::milliseconds(1), kMaxFlushInterval))
{
    if (IsBuffered())
    {
        Logger::info("Writer buffering enabled: {} bytes per thread, flush interval {} ms, drop on full: {}", m_capacity,
                     m_flushInterval.count(), m_dropOnFull);
        m_drainThread = std::thread(&WriterState::DrainLoop, this);
    }
}

WriterState::~WriterState() { Close(); }

void WriterState::Close()
{
    std::lock_guard<std::mutex> close(m_closeMutex);

    // 1. Stop and join the drain thread.
    {
        std::lock_guard<std::mutex> lock(m_drainMutex);
        m_stop = true;
    }
    m_drainCv.notify_all();
    if (m_drainThread.joinable())
    {
        m_drainThread.join();
    }

    // 2. Send and close every buffer. Registration stops first, so no new buffer can appear.
    std::lock_guard<std::mutex> pass(m_passMutex);
    std::vector<std::shared_ptr<ThreadBuffer>> buffers;
    {
        std::lock_guard<std::mutex> lock(m_rosterMutex);
        m_rosterClosed = true;
        buffers.swap(m_roster);
    }
    for (const auto& buffer : buffers)
    {
        CloseBuffer(*buffer);
    }

    // 3. Close the transport. Every buffer is closed, so nothing can send to it anymore.
    CloseTransport();
}

// ---- Transport ----

WriteResult WriterState::SendPayload(std::string_view payload)
{
    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (m_transportClosed)
    {
        return WriteResult::Closed;
    }
    try
    {
        m_transport->Write(payload);
    }
    catch (const std::exception& e)
    {
        Logger::error("Writer: failed to send {} bytes: {}", payload.size(), e.what());
    }
    return WriteResult::Consumed;
}

void WriterState::CloseTransport()
{
    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (m_transportClosed)
    {
        return;
    }
    m_transportClosed = true;
    try
    {
        m_transport->Close();
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to close writer: {}", e.what());
    }
}

// ---- Unbuffered path ----

WriteResult WriterState::WriteLine(std::string_view line, std::string& scratch)
{
    scratch.assign(line);
    scratch.push_back('\n');
    return SendPayload(scratch);
}

// ---- Buffered path ----

std::shared_ptr<WriterState::ThreadBuffer> WriterState::RegisterBuffer()
{
    if (!IsBuffered())
    {
        return nullptr;
    }
    auto buffer = std::make_shared<ThreadBuffer>();
    std::lock_guard<std::mutex> lock(m_rosterMutex);
    if (m_rosterClosed)
    {
        return nullptr;
    }
    m_roster.push_back(buffer);
    return buffer;
}

WriteResult WriterState::Append(ThreadBuffer& buffer, std::string_view line)
{
    const std::size_t bytesNeeded = line.size() + 1;  // the line and its newline

    std::unique_lock<std::mutex> data(buffer.dataMutex);
    if (buffer.closed.load(std::memory_order_relaxed))
    {
        return WriteResult::Closed;
    }

    if (buffer.pending.size() + bytesNeeded <= m_capacity)
    {
        AddToPending(buffer, line);
    }
    else if (bytesNeeded > m_capacity)
    {
        SendOversizedLine(buffer, line);
    }
    else if (m_dropOnFull)
    {
        data.unlock();
        RecordDroppedLine();
    }
    else
    {
        SendPendingThenAdd(buffer, line);
    }
    return WriteResult::Consumed;
}

void WriterState::AddToPending(ThreadBuffer& buffer, std::string_view line)
{
    buffer.pending.append(line);
    buffer.pending.push_back('\n');
}

void WriterState::SendOversizedLine(ThreadBuffer& buffer, std::string_view line)
{
    std::lock_guard<std::mutex> send(buffer.sendMutex);
    SendAndClearPending(buffer);

    std::string payload;
    payload.reserve(line.size() + 1);
    payload.append(line);
    payload.push_back('\n');
    SendPayload(payload);
}

void WriterState::SendPendingThenAdd(ThreadBuffer& buffer, std::string_view line)
{
    std::lock_guard<std::mutex> send(buffer.sendMutex);
    SendAndClearPending(buffer);
    AddToPending(buffer, line);
}

void WriterState::SendAndClearPending(ThreadBuffer& buffer)
{
    if (!buffer.pending.empty())
    {
        SendPayload(buffer.pending);
        buffer.pending.clear();
    }
}

void WriterState::CloseBuffer(ThreadBuffer& buffer)
{
    std::lock_guard<std::mutex> data(buffer.dataMutex);
    if (buffer.closed.load(std::memory_order_relaxed))
    {
        return;
    }
    {
        std::lock_guard<std::mutex> send(buffer.sendMutex);
        SendAndClearPending(buffer);
    }
    std::string().swap(buffer.pending);
    buffer.closed.store(true, std::memory_order_release);
}

// ---- Draining ----

void WriterState::DrainBuffer(ThreadBuffer& buffer, std::string& scratch)
{
    std::unique_lock<std::mutex> data(buffer.dataMutex);
    if (buffer.closed.load(std::memory_order_relaxed) || buffer.pending.empty())
    {
        return;
    }
    scratch.assign(buffer.pending);
    buffer.pending.clear();

    // Take sendMutex before releasing dataMutex, so a later flush by the owning thread can't overtake this content.
    std::lock_guard<std::mutex> send(buffer.sendMutex);
    data.unlock();
    SendPayload(scratch);
}

void WriterState::RunDrainPass()
{
    std::lock_guard<std::mutex> pass(m_passMutex);
    {
        std::lock_guard<std::mutex> lock(m_rosterMutex);
        m_snapshot.assign(m_roster.begin(), m_roster.end());
    }
    for (const auto& buffer : m_snapshot)
    {
        DrainBuffer(*buffer, m_scratch);
    }
    m_snapshot.clear();

    std::lock_guard<std::mutex> lock(m_rosterMutex);
    std::erase_if(m_roster, [](const std::shared_ptr<ThreadBuffer>& buffer)
                  { return buffer->closed.load(std::memory_order_acquire); });
}

void WriterState::RecordDroppedLine()
{
    m_dropped.fetch_add(1, std::memory_order_relaxed);
    if (m_earlyDrainRequested.exchange(true, std::memory_order_acq_rel))
    {
        return;
    }
    // Taking the mutex orders this request against the drain thread's predicate check, so a drain thread that has
    // just checked and is about to wait still sees the notification.
    {
        std::lock_guard<std::mutex> lock(m_drainMutex);
    }
    m_drainCv.notify_one();
}

void WriterState::DrainLoop()
{
    std::uint64_t reportedDrops = 0;
    auto lastWarning = std::chrono::steady_clock::now();

    std::unique_lock<std::mutex> lock(m_drainMutex);
    while (!m_stop)
    {
        const auto deadline = std::chrono::steady_clock::now() + m_flushInterval;
        m_drainCv.wait_until(lock, deadline,
                             [this] { return m_stop || m_earlyDrainRequested.load(std::memory_order_acquire); });
        if (m_stop)
        {
            break;
        }
        // Cleared before the pass, so a drop during the pass asks for another one.
        m_earlyDrainRequested.store(false, std::memory_order_release);

        lock.unlock();
        RunDrainPass();
        WarnAboutDrops(reportedDrops, lastWarning);
        lock.lock();
    }
}

void WriterState::WarnAboutDrops(std::uint64_t& reported, std::chrono::steady_clock::time_point& lastWarning)
{
    // At most one warning per flush interval, so early drains under sustained overload don't flood the log.
    const auto now = std::chrono::steady_clock::now();
    const auto dropped = DroppedCount();
    if (dropped == reported || now - lastWarning < m_flushInterval)
    {
        return;
    }
    Logger::warn("Writer dropped {} lines because a thread's buffer was full", dropped - reported);
    reported = dropped;
    lastWarning = now;
}

std::size_t WriterState::BufferCount() const
{
    std::lock_guard<std::mutex> lock(m_rosterMutex);
    return m_roster.size();
}

}  // namespace spectator::detail
