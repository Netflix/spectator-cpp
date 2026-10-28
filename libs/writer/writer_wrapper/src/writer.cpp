#include <writer.h>

#include <logger.h>

#include <algorithm>
#include <exception>
#include <new>
#include <stdexcept>

namespace spectator {

namespace detail {

// One thread's pending lines. Two mutexes keep a thread's lines in the order they were written without making appends
// wait for I/O:
// - dataMutex guards `pending` and `closed`. It is held only for short work, except by the owning thread while it
//   sends its own full buffer, and by RetireBuffer.
// - sendMutex is taken, while dataMutex is still held, by whoever takes the pending content, and held until that
//   content is sent. Content taken later therefore can't be sent first.
struct ThreadBuffer
{
    std::mutex dataMutex;
    std::mutex sendMutex;
    std::string pending;
    // Written under dataMutex. Read without it by the drain pass when it forgets closed buffers.
    std::atomic<bool> closed{false};
};

}  // namespace detail

using detail::ThreadBuffer;

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

std::unique_ptr<BaseWriter> MakeTransport(WriterType type, const std::string& param, int port)
{
    switch (type)
    {
        case WriterType::Memory:
            Logger::info("WriterWrapper initialized as MemoryWriter");
            return std::make_unique<MemoryWriter>();
        case WriterType::UDP:
            Logger::info("WriterWrapper initialized as UDPWriter with host: {} and port: {}", param, port);
            return std::make_unique<UDPWriter>(param, port);
        case WriterType::Unix:
            Logger::info("WriterWrapper initialized as UnixWriter with socket path: {}", param);
            return std::make_unique<UDSWriter>(param);
        case WriterType::Noop:
            Logger::info("WriterWrapper initialized as NoopWriter");
            return std::make_unique<NoopWriter>();
    }
    throw std::runtime_error("Unsupported writer type");
}

// Only the socket writers buffer. Memory writes must be visible as soon as they happen, and the none writer has
// nothing to send.
BufferConfig EffectiveBufferConfig(WriterType type, BufferConfig config)
{
    const bool canBuffer = type == WriterType::UDP || type == WriterType::Unix;
    if (!canBuffer && config.size > 0)
    {
        Logger::info("Writer: the {} writer does not buffer; every line is sent as it is written",
                     WriterTypeToString(type));
        config.size = 0;
    }
    return config;
}

// Trivially destructible, so it stays readable while other thread_local destructors run.
thread_local bool t_handleGone = false;

}  // namespace

// ---- Thread handle and exit guard ----

// What the calling thread keeps: a reference to its registered buffer, and a guard that flushes the buffer when the
// thread exits. One exists per thread, created on its first write.
struct Writer::ThreadHandle
{
    // Appends `line` to the calling thread's buffer, registering the buffer on the thread's first buffered write.
    static void Write(Writer& writer, std::string_view line)
    {
        if (t_handleGone)
        {
            // This thread's handle has already run its cleanup, so a new buffer would never be flushed.
            writer.SendDirect(line);
            return;
        }
        Current().WriteVia(writer, line);
    }

    // Forgets the calling thread's buffer. The Writer closed it already.
    static void DisconnectCurrentThread() noexcept
    {
        if (!t_handleGone)
        {
            Current().buffer.reset();
        }
    }

    ~ThreadHandle()
    {
        if (buffer)
        {
            GetInstance().RetireBuffer(*buffer);
        }
        t_handleGone = true;
    }

    ThreadHandle(const ThreadHandle&) = delete;
    ThreadHandle& operator=(const ThreadHandle&) = delete;
    ThreadHandle(ThreadHandle&&) = delete;
    ThreadHandle& operator=(ThreadHandle&&) = delete;

   private:
    ThreadHandle() = default;

    static ThreadHandle& Current()
    {
        thread_local ThreadHandle handle;
        return handle;
    }

    void WriteVia(Writer& writer, std::string_view line)
    {
        if (!buffer)
        {
            if (!writer.m_configured.load(std::memory_order_acquire))
            {
                writer.WarnNotInitialized();
                return;
            }
            if (!writer.IsBuffered())
            {
                writer.WriteLine(line, scratch);
                return;
            }
            buffer = writer.RegisterBuffer();
            if (!buffer)
            {
                return;  // the writer has shut down
            }
        }
        writer.Append(*buffer, line);
    }

    // Null until the first buffered write, and always null when unbuffered.
    std::shared_ptr<ThreadBuffer> buffer;
    // Reused by unbuffered sends.
    std::string scratch;
};

// Sends every pending line during static destruction.
struct Writer::ExitGuard
{
    ExitGuard() = default;
    ExitGuard(const ExitGuard&) = delete;
    ExitGuard& operator=(const ExitGuard&) = delete;
    ~ExitGuard() { Writer::ShutdownAtExit(); }
};

// ---- Configuration ----

Writer& Writer::GetInstance()
{
    static Writer* const instance = []
    {
        // Construct the logger first, so it is destroyed after ExitGuard, which may log.
        Logger::GetLogger();
        return new Writer();
    }();
    // Constructed after the logger, so it runs before the logger is destroyed.
    static ExitGuard exitGuard;
    return *instance;
}

void Writer::Initialize(WriterType type, const std::string& param, int port, const BufferConfig& buffer)
{
    Writer& writer = GetInstance();
    if (!writer.CanInitialize())
    {
        Logger::warn("Writer is already initialized or has shut down; ignoring the new configuration");
        return;
    }
    try
    {
        writer.Configure(MakeTransport(type, param, port), EffectiveBufferConfig(type, buffer));
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to initialize writer: {}", e.what());
        throw;
    }
}

bool Writer::CanInitialize() const noexcept
{
    // After the exit flush nothing would send a new buffer, so a Registry created that late is ignored too.
    return !m_configured.load(std::memory_order_acquire) && !m_exited.load(std::memory_order_relaxed);
}

void Writer::Configure(std::unique_ptr<BaseWriter> transport, const BufferConfig& config)
{
    m_transport = std::move(transport);
    m_capacity = ChooseCapacity(config.size, m_transport->MaxPayloadSize());
    m_dropOnFull = config.dropOnFull;
    m_flushInterval = std::clamp(config.flushInterval, std::chrono::milliseconds(1), kMaxFlushInterval);

    if (IsBuffered())
    {
        Logger::info("Writer buffering enabled: {} bytes per thread, flush interval {} ms, drop on full: {}", m_capacity,
                     m_flushInterval.count(), m_dropOnFull);
        try
        {
            m_drainThread = std::thread(&Writer::DrainLoop, this);
        }
        catch (...)
        {
            m_capacity = 0;
            m_transport.reset();
            throw;
        }
    }
    m_configured.store(true, std::memory_order_release);
}

// ---- Writing ----

void Writer::Write(std::string_view line) { ThreadHandle::Write(GetInstance(), line); }

void Writer::WriteLine(std::string_view line, std::string& scratch)
{
    scratch.assign(line);
    scratch.push_back('\n');
    SendPayload(scratch);
}

void Writer::SendDirect(std::string_view line)
{
    if (!m_configured.load(std::memory_order_acquire))
    {
        return;
    }
    std::string payload;
    WriteLine(line, payload);
}

std::shared_ptr<ThreadBuffer> Writer::RegisterBuffer()
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

void Writer::Append(ThreadBuffer& buffer, std::string_view line)
{
    const std::size_t bytesNeeded = line.size() + 1;  // the line and its newline

    std::unique_lock<std::mutex> data(buffer.dataMutex);
    if (buffer.closed.load(std::memory_order_relaxed))
    {
        return;
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
}

void Writer::AddToPending(ThreadBuffer& buffer, std::string_view line)
{
    buffer.pending.append(line);
    buffer.pending.push_back('\n');
}

void Writer::SendOversizedLine(ThreadBuffer& buffer, std::string_view line)
{
    std::lock_guard<std::mutex> send(buffer.sendMutex);
    SendAndClearPending(buffer);

    std::string payload;
    payload.reserve(line.size() + 1);
    payload.append(line);
    payload.push_back('\n');
    SendPayload(payload);
}

void Writer::SendPendingThenAdd(ThreadBuffer& buffer, std::string_view line)
{
    std::lock_guard<std::mutex> send(buffer.sendMutex);
    SendAndClearPending(buffer);
    AddToPending(buffer, line);
}

void Writer::SendAndClearPending(ThreadBuffer& buffer)
{
    if (!buffer.pending.empty())
    {
        SendPayload(buffer.pending);
        buffer.pending.clear();
    }
}

// ---- Draining ----

void Writer::RunDrainPass()
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

void Writer::DrainBuffer(ThreadBuffer& buffer, std::string& scratch)
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

void Writer::RecordDroppedLine()
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

void Writer::DrainLoop()
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

void Writer::WarnAboutDrops(std::uint64_t& reported, std::chrono::steady_clock::time_point& lastWarning)
{
    // At most one warning per flush interval, so early drains under sustained overload don't flood the log.
    const auto now = std::chrono::steady_clock::now();
    const auto dropped = DroppedLines();
    if (dropped == reported || now - lastWarning < m_flushInterval)
    {
        return;
    }
    Logger::warn("Writer dropped {} lines because a thread's buffer was full", dropped - reported);
    reported = dropped;
    lastWarning = now;
}

// ---- Thread exit ----

void Writer::RetireBuffer(ThreadBuffer& buffer)
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

// ---- Shutdown ----

Writer::~Writer() { Shutdown(); }

void Writer::Shutdown()
{
    std::lock_guard<std::mutex> shutdown(m_shutdownMutex);

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
        RetireBuffer(*buffer);
    }

    // 3. Close the transport. Every buffer is closed, so nothing can send to it anymore.
    CloseTransport();
}

void Writer::ShutdownAtExit()
{
    Writer& writer = GetInstance();
    writer.m_exited.store(true, std::memory_order_relaxed);
    writer.Shutdown();
}

// ---- Transport ----

void Writer::SendPayload(std::string_view payload)
{
    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (m_transportClosed)
    {
        return;
    }
    try
    {
        m_transport->Write(payload);
    }
    catch (const std::exception& e)
    {
        Logger::error("Writer: failed to send {} bytes: {}", payload.size(), e.what());
    }
}

void Writer::CloseTransport()
{
    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (m_transportClosed)
    {
        return;
    }
    m_transportClosed = true;
    if (!m_transport)
    {
        return;
    }
    try
    {
        m_transport->Close();
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to close writer: {}", e.what());
    }
}

void Writer::WarnNotInitialized()
{
    if (m_exited.load(std::memory_order_relaxed) || m_warnedNotInitialized.exchange(true))
    {
        return;
    }
    Logger::warn("Writer is not initialized; metrics are dropped until a Registry is created");
}

// ---- Test hooks ----

void Writer::InitializeWithTransport(std::unique_ptr<BaseWriter> transport, const BufferConfig& buffer)
{
    GetInstance().Configure(std::move(transport), buffer);
}

BaseWriter* Writer::GetImpl() { return GetInstance().m_transport.get(); }

void Writer::Flush() { GetInstance().RunDrainPass(); }

std::uint64_t Writer::DroppedCount() { return GetInstance().DroppedLines(); }

std::size_t Writer::RegisteredBufferCount() { return GetInstance().BufferCount(); }

std::size_t Writer::BufferCount() const
{
    std::lock_guard<std::mutex> lock(m_rosterMutex);
    return m_roster.size();
}

void Writer::ResetForTesting()
{
    Writer& writer = GetInstance();
    ThreadHandle::DisconnectCurrentThread();
    // Destroying the writer runs the shutdown sequence. The new one reuses its storage, so the pointer that
    // GetInstance returns stays valid.
    writer.~Writer();
    new (&writer) Writer();
}

}  // namespace spectator
