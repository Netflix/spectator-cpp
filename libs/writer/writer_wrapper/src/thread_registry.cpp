#include <thread_registry.h>

#include <logger.h>

#include <algorithm>

namespace spectator::detail {

namespace {

// Keeps `now + interval` far from overflowing however large the configured interval is.
constexpr std::chrono::milliseconds kMaxFlushInterval = std::chrono::hours(24);

RegistryOptions ClampFlushInterval(RegistryOptions options)
{
    options.flushInterval = std::clamp(options.flushInterval, std::chrono::milliseconds(1), kMaxFlushInterval);
    return options;
}

}  // namespace

ThreadRegistry::ThreadRegistry(std::shared_ptr<Sink> sink, const RegistryOptions& options)
    : m_sink(std::move(sink)), m_options(ClampFlushInterval(options))
{
    m_drainThread = std::thread(&ThreadRegistry::DrainLoop, this);
}

ThreadRegistry::~ThreadRegistry() { Close(); }

std::shared_ptr<ThreadBuffer> ThreadRegistry::CreateBuffer()
{
    // The owning thread calls OnLineDropped while it holds a reference to this registry, so `this` is valid.
    auto buffer = std::make_shared<ThreadBuffer>(m_sink, m_options.capacity, m_options.dropOnFull,
                                                 [this] { OnLineDropped(); });
    std::lock_guard<std::mutex> lock(m_rosterMutex);
    if (m_closed)
    {
        return nullptr;
    }
    m_roster.push_back(buffer);
    return buffer;
}

void ThreadRegistry::OnLineDropped()
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

void ThreadRegistry::RunDrainPass()
{
    std::lock_guard<std::mutex> pass(m_passMutex);
    {
        std::lock_guard<std::mutex> lock(m_rosterMutex);
        m_snapshot.assign(m_roster.begin(), m_roster.end());
    }
    for (const auto& buffer : m_snapshot)
    {
        buffer->SendPending(m_scratch);
    }
    m_snapshot.clear();

    std::lock_guard<std::mutex> lock(m_rosterMutex);
    std::erase_if(m_roster, [](const std::shared_ptr<ThreadBuffer>& buffer) { return buffer->IsClosed(); });
}

void ThreadRegistry::Close()
{
    std::lock_guard<std::mutex> close(m_closeMutex);
    {
        std::lock_guard<std::mutex> lock(m_drainMutex);
        m_stop = true;
    }
    m_drainCv.notify_all();
    if (m_drainThread.joinable())
    {
        m_drainThread.join();
    }

    std::lock_guard<std::mutex> pass(m_passMutex);
    std::vector<std::shared_ptr<ThreadBuffer>> buffers;
    {
        std::lock_guard<std::mutex> lock(m_rosterMutex);
        m_closed = true;
        buffers.swap(m_roster);
    }
    for (const auto& buffer : buffers)
    {
        buffer->FlushAndClose();
    }
}

std::size_t ThreadRegistry::BufferCount() const
{
    std::lock_guard<std::mutex> lock(m_rosterMutex);
    return m_roster.size();
}

void ThreadRegistry::DrainLoop()
{
    std::uint64_t reportedDrops = 0;
    auto lastWarning = std::chrono::steady_clock::now();

    std::unique_lock<std::mutex> lock(m_drainMutex);
    while (!m_stop)
    {
        const auto deadline = std::chrono::steady_clock::now() + m_options.flushInterval;
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

void ThreadRegistry::WarnAboutDrops(std::uint64_t& reported, std::chrono::steady_clock::time_point& lastWarning)
{
    // At most one warning per flush interval, so early drains under sustained overload don't flood the log.
    const auto now = std::chrono::steady_clock::now();
    const auto dropped = DroppedCount();
    if (dropped == reported || now - lastWarning < m_options.flushInterval)
    {
        return;
    }
    Logger::warn("Writer dropped {} lines because a thread's buffer was full", dropped - reported);
    reported = dropped;
    lastWarning = now;
}

}  // namespace spectator::detail
