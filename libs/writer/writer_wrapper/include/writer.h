#pragma once

#include <buffer_config.h>
#include <writer_types.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace spectator {

namespace detail {
// One thread's pending lines and the locks that guard them. Defined in writer.cpp.
struct ThreadBuffer;
}  // namespace detail

// The process-wide writer every meter sends its protocol lines through. It is configured once, then owns the
// transport, the registry of per-thread buffers, and the drain thread.
//
//   Writer
//    +-- transport             every send goes through one mutex
//    +-- drain thread          sends the buffers on an interval
//    +-- registry of buffers   one ThreadBuffer per writing thread
//
//   Each writing thread
//    +-- a reference to its registered buffer, plus a small guard that flushes it when the thread exits
//
// Unbuffered (buffer size 0, and always for the memory and none writers), each line goes straight to the transport.
// Buffered, each thread appends to its own buffer. A buffer is sent when it fills (or, with drop-on-full, the line is
// dropped and the drain thread is woken), when the drain thread runs every flush interval, and when its thread exits.
// A thread's lines always arrive in the order it wrote them; lines from different threads can arrive up to one flush
// interval out of order.
//
// The flows:
//   Write        obtain this thread's buffer, then append
//   Drain        visit the registered buffers, then send their pending bytes
//   Thread exit  flush and retire the thread's buffer
//   Shutdown     stop the drain thread, flush every buffer, then close the transport
//
// Locks, in the order they may be taken: shutdownMutex, passMutex, rosterMutex, a buffer's dataMutex, that buffer's
// sendMutex, transportMutex. drainMutex is never held while taking another.
class Writer final
{
   public:
    // Runs the shutdown sequence. The process-wide instance is never destroyed, so this only runs for the standalone
    // writers that tests create.
    ~Writer();

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&) = delete;
    Writer& operator=(Writer&&) = delete;

   private:
    friend class Registry;
    friend class WriterTestHelper;
    friend class AgeGauge;
    friend class Counter;
    friend class DistributionSummary;
    friend class Gauge;
    friend class MaxGauge;
    friend class MonotonicCounter;
    friend class MonotonicCounterUint;
    friend class PercentileDistributionSummary;
    friend class PercentileTimer;
    friend class Timer;

    struct ThreadHandle;
    struct ExitGuard;

    Writer() = default;

    // ---- Entry points ----

    // Configures the writer. The writer is configured once per process, by one thread: if it already has a
    // configuration, this logs a warning and the new settings are ignored. Throws if the transport can't be built.
    static void Initialize(WriterType type, const std::string& param, int port, const BufferConfig& buffer);

    // Sends one protocol line. The writer adds the trailing newline. Meters call this.
    static void Write(std::string_view line);

    // ---- Configuration ----

    // The process-wide instance. It is never destroyed, so threads still running during static destruction can call it
    // safely. An exit guard sends pending lines instead.
    static Writer& GetInstance();

    // False once the writer is configured, or has shut down at exit.
    bool CanInitialize() const noexcept;

    // Takes ownership of the transport and starts the drain thread if buffering is on. Called once. Publishes the
    // configuration to writing threads with a release store of m_configured.
    void Configure(std::unique_ptr<BaseWriter> transport, const BufferConfig& config);

    [[nodiscard]] bool IsBuffered() const noexcept { return m_capacity > 0; }

    // ---- Writing ----

    // Unbuffered: sends `line` and a newline as one datagram. `scratch` holds the combined payload so repeated calls
    // don't allocate.
    void WriteLine(std::string_view line, std::string& scratch);

    // For writes that arrive after the calling thread's handle has run its cleanup, such as from a late thread_local
    // destructor. A new buffer would never be flushed, so the line is sent on its own.
    void SendDirect(std::string_view line);

    // Buffered: a new buffer for the calling thread. Null if the writer is unbuffered or has shut down.
    std::shared_ptr<detail::ThreadBuffer> RegisterBuffer();

    // Adds `line` and a newline to the thread's own buffer, in this order of precedence:
    //   1. It fits: it is added.
    //   2. It alone is larger than the whole buffer: pending lines are sent, then the line is sent on its own. It is
    //      never dropped or truncated.
    //   3. The buffer is full and drop-on-full is set: the line is dropped, counted, and the drain thread is woken.
    //   4. The buffer is full: the calling thread sends the pending lines, then adds the line.
    // Only the buffer's owning thread calls this. Lines for a closed buffer are dropped.
    void Append(detail::ThreadBuffer& buffer, std::string_view line);

    // Each of these requires the lock or locks named in its comment.

    // buffer.dataMutex.
    void AddToPending(detail::ThreadBuffer& buffer, std::string_view line);
    // buffer.dataMutex.
    void SendOversizedLine(detail::ThreadBuffer& buffer, std::string_view line);
    // buffer.dataMutex.
    void SendPendingThenAdd(detail::ThreadBuffer& buffer, std::string_view line);
    // buffer.dataMutex and buffer.sendMutex.
    void SendAndClearPending(detail::ThreadBuffer& buffer);

    // ---- Draining ----

    // One drain pass on the calling thread: send every buffer's pending lines, then forget closed buffers.
    void RunDrainPass();

    // None; takes the buffer's locks.
    void DrainBuffer(detail::ThreadBuffer& buffer, std::string& scratch);

    // Counts a dropped line and wakes the drain thread, so the full buffer is emptied now rather than at the next
    // interval.
    void RecordDroppedLine();

    void DrainLoop();
    void WarnAboutDrops(std::uint64_t& reported, std::chrono::steady_clock::time_point& lastWarning);

    // ---- Thread exit ----

    // Sends the buffer's pending lines, frees its memory, and closes it. Called when the owning thread exits. Safe to
    // call more than once.
    void RetireBuffer(detail::ThreadBuffer& buffer);

    // ---- Shutdown ----

    // Stops and joins the drain thread, sends and closes every buffer, then closes the transport. Safe to call more
    // than once, from any thread that is not the drain thread.
    void Shutdown();

    // Called at static destruction: shuts down and refuses further Initialize calls. Writes are dropped afterwards.
    static void ShutdownAtExit();

    // ---- Transport ----

    // Serializes every transport write. Does nothing once the transport is closed.
    void SendPayload(std::string_view payload);
    void CloseTransport();

    void WarnNotInitialized();

    // ---- Test hooks, reached through WriterTestHelper ----

    static void InitializeWithTransport(std::unique_ptr<BaseWriter> transport, const BufferConfig& buffer);
    static BaseWriter* GetImpl();
    static void Flush();
    static std::uint64_t DroppedCount();
    static std::size_t RegisteredBufferCount();

    // Replaces the process-wide writer with a fresh, unconfigured one, so the next Initialize starts clean. Sends and
    // closes the old one first, and disconnects the calling thread from its buffer. No other thread may be writing.
    static void ResetForTesting();

    std::uint64_t DroppedLines() const noexcept { return m_dropped.load(std::memory_order_relaxed); }
    std::size_t BufferCount() const;

    // ---- State ----

    // Set, with release ordering, once the settings and transport below are in place. Threads that see it can read them.
    std::atomic<bool> m_configured{false};
    std::atomic<bool> m_exited{false};
    std::atomic<bool> m_warnedNotInitialized{false};

    // Settings. A capacity of 0 means unbuffered.
    std::size_t m_capacity = 0;
    bool m_dropOnFull = false;
    std::chrono::milliseconds m_flushInterval{5000};

    // Transport.
    std::mutex m_transportMutex;
    std::unique_ptr<BaseWriter> m_transport;
    bool m_transportClosed = false;

    // Registry of buffers.
    mutable std::mutex m_rosterMutex;
    std::vector<std::shared_ptr<detail::ThreadBuffer>> m_roster;
    bool m_rosterClosed = false;

    // Drain passes share the scratch string and the snapshot, so they are serialized.
    std::mutex m_passMutex;
    std::string m_scratch;
    std::vector<std::shared_ptr<detail::ThreadBuffer>> m_snapshot;

    std::atomic<std::uint64_t> m_dropped{0};
    // Set by a dropped line, cleared by the drain thread when it starts a pass.
    std::atomic<bool> m_earlyDrainRequested{false};

    std::mutex m_shutdownMutex;

    // m_stop is written only while m_drainMutex is held, so the drain thread can't miss a stop between checking it
    // and waiting.
    std::mutex m_drainMutex;
    std::condition_variable m_drainCv;
    bool m_stop = false;

    // Declared last, and started at the end of Configure, so it never sees a member that isn't constructed.
    std::thread m_drainThread;
};

}  // namespace spectator
