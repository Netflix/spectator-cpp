#pragma once

#include <sink.h>

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace spectator::detail {

// One writing thread's accumulation buffer. The owning thread appends newline-terminated lines. The drain thread,
// reconfiguration and the thread's exit take whatever is pending and send it through the sink.
//
// Appending a line has four outcomes, in this order of precedence:
//   1. It fits in the buffer: it is added.
//   2. The line alone is larger than the whole buffer: pending lines are sent, then the line is sent on its own. It is
//      never dropped or truncated.
//   3. The buffer is full and drop-on-full is set: the line is dropped, and onDrop is called.
//   4. The buffer is full: the owning thread sends the pending lines, then adds the line.
//
// Two mutexes keep a thread's lines in the order they were written without making appends wait for I/O:
// - dataMutex guards `pending` and the closed flag. It is held only for short work, except by the owning thread
//   while it sends its own full buffer, and by Close.
// - sendMutex is taken, while dataMutex is still held, by whoever takes the pending content, and held until that
//   content is sent. Content taken later therefore can't be sent first.
// Lock order: dataMutex, then sendMutex, then the sink's mutex.
class ThreadBuffer
{
   public:
    enum class AppendResult
    {
        Appended,  // buffered, or sent if the buffer was full or the line alone was larger than the buffer
        Dropped,   // the buffer was full and drop-on-full is set; the line was discarded and onDrop was called
        Closed     // the buffer is closed; the caller must attach to the current configuration and retry
    };

    // onDrop, if set, is called by the owning thread, with no lock held, each time a line is dropped.
    ThreadBuffer(std::shared_ptr<Sink> sink, std::size_t capacity, bool dropOnFull, std::function<void()> onDrop = {});

    ThreadBuffer(const ThreadBuffer&) = delete;
    ThreadBuffer& operator=(const ThreadBuffer&) = delete;
    ThreadBuffer(ThreadBuffer&&) = delete;
    ThreadBuffer& operator=(ThreadBuffer&&) = delete;

    // Adds `line` and a newline. Only the owning thread calls this.
    AppendResult Append(std::string_view line);

    // Sends what is pending. The content is copied into `scratch` first, so the owning thread can keep appending
    // while it is sent. Called by the drain thread.
    void SendPending(std::string& scratch);

    // Sends what is pending, then closes the buffer and frees its memory; later appends return Closed. Called when
    // the owning thread exits and when the writer is reconfigured or shut down. Safe to call more than once.
    void FlushAndClose();

    [[nodiscard]] bool IsClosed() const noexcept { return m_closed.load(std::memory_order_acquire); }

   private:
    // Each of these requires the lock or locks named in its comment.

    // dataMutex.
    void AddToPending(std::string_view line);
    // dataMutex. Sends pending lines, then the line alone.
    void SendOversizedLine(std::string_view line);
    // dataMutex. Sends pending lines, then adds the line to the emptied buffer.
    void SendPendingThenAdd(std::string_view line);
    // dataMutex and sendMutex.
    void SendAndClearPending();

    const std::shared_ptr<Sink> m_sink;
    const std::size_t m_capacity;
    const bool m_dropOnFull;
    const std::function<void()> m_onDrop;

    std::mutex m_dataMutex;
    std::mutex m_sendMutex;
    std::string m_pending;
    // Written under dataMutex. Read without it by the drain thread when it forgets closed buffers.
    std::atomic<bool> m_closed{false};
};

}  // namespace spectator::detail
