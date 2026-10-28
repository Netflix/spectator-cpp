#pragma once

// Private to the writer library. Only writer.cpp and the tests include it.

#include <base_writer.h>
#include <buffer_config.h>

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

namespace spectator::detail {

// What happened to a line handed to a WriterState.
enum class WriteResult
{
    Consumed,  // sent, buffered, or deliberately dropped: don't retry. This does not mean it was delivered.
    Closed     // the state was closed by a reconfigure or shutdown: reconnect to the current state before retrying
};

// One writer configuration: the transport and everything that moves lines to it.
//
// Unbuffered (buffer size 0), WriteLine sends each line straight to the transport.
//
// Buffered, each writing thread registers a ThreadBuffer and appends lines to it. A buffer's lines are sent when it
// fills, when the drain thread runs, and when its thread closes it at exit. The drain thread runs every flush interval,
// or sooner when a line is dropped, and forgets the buffers of threads that have exited.
//
// Ownership: the Writer and every connected thread share the state, and the state and that thread share the thread's
// buffer. A buffer never refers back to the state. Closing the state closes every buffer, so a thread that still holds
// the state gets Closed on its next write and reconnects to the Writer's current state.
//
// Locks, in the order they may be taken: closeMutex, passMutex, rosterMutex, a buffer's dataMutex, that buffer's
// sendMutex, transportMutex. drainMutex is never held while taking another.
class WriterState
{
   public:
    // One thread's pending bytes and the locks that guard them. Defined in writer_state.cpp.
    struct ThreadBuffer;

    // Takes ownership of the transport. If config.size > 0, clamps the buffer size to the transport's largest
    // datagram and starts the drain thread; that is the only thing that can throw.
    WriterState(std::unique_ptr<BaseWriter> transport, const BufferConfig& config);

    // Closes the state.
    ~WriterState();

    WriterState(const WriterState&) = delete;
    WriterState& operator=(const WriterState&) = delete;
    WriterState(WriterState&&) = delete;
    WriterState& operator=(WriterState&&) = delete;

    [[nodiscard]] bool IsBuffered() const noexcept { return m_capacity > 0; }

    // ---- Unbuffered path ----

    // Sends `line` and a newline as one datagram. `scratch` holds the combined payload so repeated calls don't
    // allocate.
    WriteResult WriteLine(std::string_view line, std::string& scratch);

    // ---- Buffered path ----

    // A new buffer for the calling thread. Null if the state is unbuffered or already closed.
    std::shared_ptr<ThreadBuffer> RegisterBuffer();

    // Adds `line` and a newline to the thread's own buffer, in this order of precedence:
    //   1. It fits: it is added.
    //   2. It alone is larger than the whole buffer: pending lines are sent, then the line is sent on its own. It is
    //      never dropped or truncated.
    //   3. The buffer is full and drop-on-full is set: the line is dropped, counted, and the drain thread is woken.
    //   4. The buffer is full: the calling thread sends the pending lines, then adds the line.
    // Only the buffer's owning thread calls this.
    WriteResult Append(ThreadBuffer& buffer, std::string_view line);

    // Sends the buffer's pending lines, frees its memory, and closes it; later appends return Closed. Called when the
    // owning thread exits. Safe to call more than once.
    void CloseBuffer(ThreadBuffer& buffer);

    // ---- Lifecycle ----

    // The complete shutdown sequence: stop and join the drain thread, send and close every buffer, then close the
    // transport. Safe to call more than once, from any thread that is not the drain thread.
    void Close();

    // One drain pass on the calling thread: send every buffer's pending lines, then forget closed buffers.
    void RunDrainPass();

    // ---- Diagnostics and tests ----

    [[nodiscard]] BaseWriter* Transport() const noexcept { return m_transport.get(); }
    [[nodiscard]] std::uint64_t DroppedCount() const noexcept { return m_dropped.load(std::memory_order_relaxed); }
    [[nodiscard]] std::size_t BufferCount() const;

   private:
    // Serializes every transport write. Returns Closed, without writing, once the transport is closed.
    WriteResult SendPayload(std::string_view payload);
    void CloseTransport();

    // Each of these requires the lock or locks named in its comment.

    // buffer.dataMutex.
    void AddToPending(ThreadBuffer& buffer, std::string_view line);
    // buffer.dataMutex.
    void SendOversizedLine(ThreadBuffer& buffer, std::string_view line);
    // buffer.dataMutex.
    void SendPendingThenAdd(ThreadBuffer& buffer, std::string_view line);
    // buffer.dataMutex and buffer.sendMutex.
    void SendAndClearPending(ThreadBuffer& buffer);
    // None; takes the buffer's locks.
    void DrainBuffer(ThreadBuffer& buffer, std::string& scratch);

    // Counts a dropped line and wakes the drain thread, so the full buffer is emptied now rather than at the next
    // interval.
    void RecordDroppedLine();

    void DrainLoop();
    void WarnAboutDrops(std::uint64_t& reported, std::chrono::steady_clock::time_point& lastWarning);

    // Transport. Declared first: the capacity is derived from it.
    std::mutex m_transportMutex;
    const std::unique_ptr<BaseWriter> m_transport;
    bool m_transportClosed = false;

    // Buffering. A capacity of 0 means unbuffered.
    const std::size_t m_capacity;
    const bool m_dropOnFull;
    const std::chrono::milliseconds m_flushInterval;

    // Roster of buffers.
    mutable std::mutex m_rosterMutex;
    std::vector<std::shared_ptr<ThreadBuffer>> m_roster;
    bool m_rosterClosed = false;

    // Drain passes share the scratch string and the snapshot, so they are serialized.
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

    // Declared last, and started at the end of the constructor, so it never sees a member that isn't constructed.
    std::thread m_drainThread;
};

}  // namespace spectator::detail
