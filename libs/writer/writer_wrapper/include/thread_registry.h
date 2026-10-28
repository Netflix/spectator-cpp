#pragma once

#include <sink.h>
#include <thread_buffer.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace spectator::detail {

struct RegistryOptions
{
    // Bytes each thread may buffer. Never more than the sink's largest payload.
    std::size_t capacity = 0;
    // Drop lines that don't fit in a full buffer, instead of having the writing thread send it.
    bool dropOnFull = false;
    // The longest a line waits in a buffer before the drain thread sends it.
    std::chrono::milliseconds flushInterval = std::chrono::seconds(5);
};

// The roster of per-thread buffers for one writer configuration, and the drain thread that empties them. The drain
// thread runs a pass every flush interval, or sooner when a dropped line asks for one. Each pass sends every buffer's
// pending lines, then forgets the buffers of threads that have exited.
class ThreadRegistry
{
   public:
    // Starts the drain thread.
    ThreadRegistry(std::shared_ptr<Sink> sink, const RegistryOptions& options);

    // Closes the registry, which stops the drain thread and sends every pending line.
    ~ThreadRegistry();

    ThreadRegistry(const ThreadRegistry&) = delete;
    ThreadRegistry& operator=(const ThreadRegistry&) = delete;
    ThreadRegistry(ThreadRegistry&&) = delete;
    ThreadRegistry& operator=(ThreadRegistry&&) = delete;

    // A new buffer for the calling thread, or null once the registry is closed.
    std::shared_ptr<ThreadBuffer> CreateBuffer();

    // One drain pass: send every buffer's pending lines, then forget buffers that are closed.
    void RunDrainPass();

    // Stops and joins the drain thread, then sends and closes every buffer. Later Register calls return null. Safe to
    // call more than once.
    void Close();

    // Buffers in the roster, including closed ones the drain thread has not forgotten yet.
    [[nodiscard]] std::size_t BufferCount() const;

    // Lines dropped because a buffer was full.
    [[nodiscard]] std::uint64_t DroppedCount() const noexcept { return m_dropped.load(std::memory_order_relaxed); }

   private:
    // Counts a dropped line and wakes the drain thread, so the full buffer is emptied now rather than at the next
    // interval. Buffers call it when they drop a line.
    void OnLineDropped();

    void DrainLoop();
    void WarnAboutDrops(std::uint64_t& reported, std::chrono::steady_clock::time_point& lastWarning);

    const std::shared_ptr<Sink> m_sink;
    const RegistryOptions m_options;

    mutable std::mutex m_rosterMutex;
    std::vector<std::shared_ptr<ThreadBuffer>> m_roster;
    bool m_closed = false;

    // Serializes drain passes, which share the scratch string and the snapshot.
    std::mutex m_passMutex;
    std::string m_scratch;
    std::vector<std::shared_ptr<ThreadBuffer>> m_snapshot;

    std::atomic<std::uint64_t> m_dropped{0};
    // Set by a dropped line, cleared by the drain thread when it starts a pass.
    std::atomic<bool> m_earlyDrainRequested{false};

    std::mutex m_closeMutex;

    // m_stop is written only while m_drainMutex is held, so the drain thread can't miss a stop between checking it
    // and waiting.
    std::mutex m_drainMutex;
    std::condition_variable m_drainCv;
    bool m_stop = false;

    // Declared last, and started in the constructor body, so it never sees a member that isn't constructed yet.
    std::thread m_drainThread;
};

}  // namespace spectator::detail
