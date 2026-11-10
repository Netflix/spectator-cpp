#include <buffered_writer.h>

#include <logger.h>

#include <pthread.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace spectator {

// Buffered mode
// =============
//
// Each thread that records has its own ThreadBuffers: the buffer it is filling, and up to kMaxWaitingBuffers full
// buffers that it has handed over and that wait for the sender. One sender thread takes the waiting buffers of all
// threads and sends each as one datagram. Recording a line normally means appending it to the calling thread's own
// buffer under that thread's own, normally uncontended, mutex: nothing that another recording thread writes is
// touched, and nothing is sent.
//
// Shared state, and who uses it:
//   - g_current: which writer is in use, and the key under which each thread finds its own buffers. Read for every
//     line, written only by initialization.
//   - the registry of all ThreadBuffers: a thread registers on its first line, the sender releases the buffers of a
//     thread that has exited once they are sent. Never used while recording a line.
//   - the wake-up flags of the sender: a thread reads them when it hands a full buffer over, see SignalSender.
//   - the socket writer and its lock: used by the sender thread, by a recording thread only under
//     FullBufferPolicy::Send, and by the thread that shuts the writer down.
//
// Order of the lines of one thread: its buffers are sent oldest first. A buffer is only ever taken out of a thread's
// waiting list by a thread that holds the socket lock and that sends the buffer before it releases that lock, and
// the buffer being filled is only taken when the waiting list is empty.
//
// Lock order: a thread's own lock is always the last lock taken, after the socket lock or the registry lock. The
// socket lock and the registry lock are never held together, and the wake-up lock is never held together with any
// other lock. The sender waits for a recording thread only to take that thread's own lock for a moment (and, under
// FullBufferPolicy::Send, for the socket lock while a recording thread sends one buffer), so a recording thread
// that waits for the sender can always be served.
//
// Thread exit: a thread's buffers are attached to it as the value of a pthread key. The destructor of that key runs
// when the thread exits, after the destructors of the thread's thread_local objects, and closes the buffers: the
// sender sends what is in them and then releases them. From then on the thread is marked as exited: in
// t_threadExited, and in the key where that flag does not last (see kMarkExitedThreadInKey). Should anything still
// record on it (the destructor of another key), that line goes straight to the sender, see RecordAfterThreadExit:
// the thread gets no buffers again, as nothing would let go of them.
//
// Process exit: the destructor of the key does not run for threads that are still running when the process exits,
// and that includes the thread that calls exit(). Their buffers stay attached and usable while thread_local and
// static destructors run. ~Writer then calls ShutdownAtExit(), which stops all buffers, joins the sender and sends
// what is left on the exiting thread. The writer itself is never destroyed, so a thread that records after that
// finds its own buffers stopped and drops the line, without touching anything that static destruction destroys.

namespace {

constexpr char kNewLine = '\n';

// A thread may have this many full buffers waiting for the sender. What it does when it needs to hand over one more
// is the FullBufferPolicy.
constexpr std::size_t kMaxWaitingBuffers = 8;

// At least the cache line size of the platforms we build for, so that the buffers of two threads never share a line.
constexpr std::size_t kCacheLineSize = 128;

// The flush interval is kept within these bounds so that the sender's timed wait is well defined for any input.
// The limit that users see is BufferingDefaults::MaxFlushInterval, which WriterConfig enforces; these bounds are
// wider on purpose, so that tests that call Start() directly can keep the timed flush out of the way.
constexpr std::chrono::milliseconds kMinFlushInterval{1};
constexpr std::chrono::milliseconds kMaxFlushInterval{std::chrono::hours{24}};

using Clock = std::chrono::steady_clock;

// A thread that has let go of its buffers at exit has to be recognized for as long as code can run on it, which is
// longest on the thread whose exit ends the process and then runs the static destructors. t_threadExited says so,
// and so can a mark in the thread key, but neither lasts everywhere:
//   - macOS releases thread_local storage while key destructors are still running and sets it up afresh when it is
//     used again, so in the later rounds of key destructors t_threadExited reads as false again. The value of a key
//     lasts there, also after the last round. So the thread is marked in the key as well.
//   - Elsewhere thread_local storage lasts until the thread is gone, while glibc clears the keys after the last
//     round. So t_threadExited alone marks the thread and the key is left empty, which also means that OnThreadExit
//     runs only once. It must not run in the last round: ThreadSanitizer on Linux lets go of the thread in that
//     round, in a key destructor that runs before this one, and crashes on instrumented code that runs on the thread
//     after that.
#if defined(__APPLE__)
constexpr bool kMarkExitedThreadInKey = true;
#else
constexpr bool kMarkExitedThreadInKey = false;
#endif

// The mark in the thread key: one of the numbers 1 to kMaxExitedRounds instead of a pointer. The number counts the
// rounds of key destructors since the thread let go of its buffers, see OnThreadExit. Every implementation runs at
// least this many rounds (_POSIX_THREAD_DESTRUCTOR_ITERATIONS).
constexpr std::uintptr_t kMaxExitedRounds = 4;

// Whether the value of the thread key says that the calling thread has no buffers: it is null because the thread
// has not recorded yet or is marked as exited by t_threadExited alone, or it is the mark of a thread that has exited.
bool HasNoBuffers(const void* value) noexcept { return reinterpret_cast<std::uintptr_t>(value) <= kMaxExitedRounds; }

// Set when the calling thread has let go of its buffers at exit.
constinit thread_local bool t_threadExited = false;

class BufferedWriter;

// The full buffers that one thread has handed over and that the sender has not taken yet, oldest first.
class WaitingBuffers
{
   public:
    bool Empty() const noexcept { return m_count == 0; }
    bool Full() const noexcept { return m_count == kMaxWaitingBuffers; }

    void Push(std::string&& buffer) noexcept
    {
        m_buffers[(m_first + m_count) % kMaxWaitingBuffers] = std::move(buffer);
        ++m_count;
    }

    std::string Pop() noexcept
    {
        std::string buffer = std::move(m_buffers[m_first]);
        m_first = (m_first + 1) % kMaxWaitingBuffers;
        --m_count;
        return buffer;
    }

   private:
    std::array<std::string, kMaxWaitingBuffers> m_buffers;
    std::size_t m_first = 0;
    std::size_t m_count = 0;
};

// The buffers of one thread. The registry of the BufferedWriter owns them. The thread finds them through the thread
// key until it exits.
struct alignas(kCacheLineSize) ThreadBuffers : std::enable_shared_from_this<ThreadBuffers>
{
    ThreadBuffers(std::shared_ptr<BufferedWriter> writer, std::size_t size, FullBufferPolicy fullBufferPolicy)
        : owner(std::move(writer)), bufferSize(size), policy(fullBufferPolicy)
    {
    }

    // The writer these buffers belong to. Holding it here keeps it alive for as long as a thread can still reach it
    // through its buffers.
    const std::shared_ptr<BufferedWriter> owner;
    // Copies of the writer's configuration, so that recording a line reads nothing else of the writer.
    const std::size_t bufferSize;
    const FullBufferPolicy policy;

    // Guards everything below. Normally only the owning thread takes it. The sender takes it for a moment when it
    // looks for buffers to send.
    std::mutex mutex;
    // FullBufferPolicy::Block: the owning thread waits here until the sender has taken one of its buffers.
    std::condition_variable room;
    // The buffer being filled. Never longer than bufferSize, unless it is a single line that is longer than that.
    std::string current;
    // Full buffers on their way to the sender.
    WaitingBuffers waiting;
    // Lines dropped under FullBufferPolicy::Drop that the sender has not reported yet.
    std::uint64_t dropped = 0;
    // The owning thread is waiting on `room`.
    bool ownerWaiting = false;
    // The writer has shut down: lines are no longer accepted.
    bool stopped = false;
    // The owning thread has let go of these buffers: nothing will be added, and the sender releases them once sent.
    bool closed = false;
};

// What every recorded line reads and only initialization writes. Constant-initialized and trivially destructible,
// so it can be used at any time, including while the process runs its static destructors, and on a cache line of
// its own, so that reading it never waits for a write to something else.
struct alignas(kCacheLineSize) Current
{
    // The buffered writer in use, or nullptr when buffering is off.
    std::atomic<BufferedWriter*> writer{nullptr};
    // The value of this key is the calling thread's ThreadBuffers, or says that it has none (see HasNoBuffers).
    // Its destructor is OnThreadExit. Created by the first Start() and never deleted; valid whenever `writer` is
    // not null.
    pthread_key_t threadKey{};
    bool threadKeyCreated = false;
};

constinit Current g_current{};

// Owns the buffered writer that g_current.writer points to. Deliberately never destroyed: threads may still be
// recording while the process runs its static destructors, and after ~Writer they have to find a writer that has
// shut down, not a destroyed one.
std::shared_ptr<BufferedWriter>& CurrentOwner()
{
    static auto* const owner = new std::shared_ptr<BufferedWriter>();
    return *owner;
}

class BufferedWriter final : public std::enable_shared_from_this<BufferedWriter>
{
   public:
    BufferedWriter(BaseWriter* writer, std::size_t bufferSize, std::chrono::milliseconds flushInterval,
                   FullBufferPolicy policy)
        : m_writer(writer),
          m_bufferSize(bufferSize),
          m_policy(policy),
          m_flushInterval(std::clamp(flushInterval, kMinFlushInterval, kMaxFlushInterval))
    {
    }

    ~BufferedWriter() { Shutdown(); }

    BufferedWriter(const BufferedWriter&) = delete;
    BufferedWriter& operator=(const BufferedWriter&) = delete;

    // Starts the sender thread.
    void Start() { m_sender = std::thread(&BufferedWriter::SenderLoop, this); }

    // Stops accepting lines, releases the threads that wait for buffer space, joins the sender thread, sends what
    // every thread still has buffered and lets go of the socket writer. Runs once; later calls return immediately.
    void Shutdown();

    // Records a line in the calling thread's own buffers. Returns false, without recording the line, if the writer
    // that the buffers belong to has shut down.
    static bool Record(ThreadBuffers& buffers, const std::string& line);

    // Records a line for a thread for which Record() is not possible or returned false: the thread has no buffers,
    // or its buffers belong to a writer that has shut down. `value` is the value of the thread key, and `this` is
    // the writer in use.
    void RecordSlow(void* value, const std::string& line);

    // The calling thread lets go of its buffers: at thread exit, or because they belong to an earlier
    // initialization. Their lines go to the sender, which releases the buffers once they are sent.
    static void Detach(ThreadBuffers* buffers);

   private:
    struct Visited
    {
        bool sent = false;      // at least one buffer was sent
        bool finished = false;  // the thread has gone and everything it recorded has been sent
    };

    ThreadBuffers* Attach();
    void RecordAfterThreadExit(const std::string& line);
    static void HandOver(ThreadBuffers& buffers) noexcept;
    void SendOldest(ThreadBuffers& buffers, std::unique_lock<std::mutex>& lock);
    void SignalSender();

    void SenderLoop();
    bool Pass(bool flush);
    Visited Visit(ThreadBuffers& buffers, bool takePartial);
    void Release(std::size_t index);
    void DrainAll();
    void ReportDropped();
    void Send(const std::string& buffer);

    // The socket writer, which is not thread-safe. The Writer singleton owns it, and shuts this writer down before
    // it replaces or destroys the socket writer. Guarded by m_socketMutex; null once this writer has shut down.
    BaseWriter* m_writer;
    const std::size_t m_bufferSize;
    const FullBufferPolicy m_policy;
    const Clock::duration m_flushInterval;

    // Held by whoever is sending.
    std::mutex m_socketMutex;

    // Guards the registry. Taken when a thread records its first line, when a thread exits and once per pass of the
    // sender; never while a line is recorded.
    std::mutex m_registryMutex;
    // All thread buffers, in the order in which they were registered.
    std::vector<std::shared_ptr<ThreadBuffers>> m_registry;
    // Buffers were registered since the sender last copied the registry.
    bool m_registryChanged = false;
    // The final drain is done. From now on a thread that lets go of its buffers removes them from the registry
    // itself, because nobody else will.
    bool m_finished = false;

    // Used by the sender thread only (and by Shutdown once the sender has been joined).
    std::vector<ThreadBuffers*> m_visiting;  // the sender's copy of the registry
    std::uint64_t m_dropped = 0;             // dropped lines collected from the threads and not logged yet

    // How a recording thread wakes the sender, see SignalSender.
    std::atomic<bool> m_workPending{false};
    std::atomic<bool> m_senderWaiting{false};
    std::mutex m_wakeMutex;
    std::condition_variable m_wakeCv;

    std::atomic<bool> m_shutdown{false};
    std::mutex m_shutdownMutex;
    bool m_shutdownDone = false;
    std::thread m_sender;
};

bool BufferedWriter::Record(ThreadBuffers& buffers, const std::string& line)
{
    const std::size_t lineSize = line.size() + 1;  // with its newline
    bool handedOver = false;
    {
        std::unique_lock<std::mutex> lock(buffers.mutex);
        for (;;)
        {
            if (buffers.stopped)
            {
                return false;
            }
            if (buffers.current.empty() || buffers.current.size() + lineSize <= buffers.bufferSize)
            {
                break;  // the line goes into the current buffer
            }

            // The line does not fit, so the current buffer has to be handed over first.
            if (!buffers.waiting.Full())
            {
                // The next buffer is allocated before the full one is handed over. Nothing after the hand-over
                // can fail, so the sender is always told about it.
                std::string next;
                next.reserve(std::max(buffers.bufferSize, lineSize));
                HandOver(buffers);
                buffers.current = std::move(next);
                handedOver = true;
                break;
            }

            // The thread has reached its limit of waiting buffers.
            if (buffers.policy == FullBufferPolicy::Drop)
            {
                ++buffers.dropped;
                return true;
            }
            if (buffers.policy == FullBufferPolicy::Send)
            {
                buffers.owner->SendOldest(buffers, lock);
            }
            else  // FullBufferPolicy::Block
            {
                buffers.ownerWaiting = true;
                buffers.room.wait(lock, [&buffers] { return buffers.stopped || !buffers.waiting.Full(); });
                buffers.ownerWaiting = false;
            }
            // The lock was released in between, and the sender may have taken any of the buffers: look again.
        }

        if (buffers.current.capacity() < buffers.current.size() + lineSize)
        {
            // The first line of a buffer, or a single line that is longer than the buffer size.
            buffers.current.reserve(std::max(buffers.bufferSize, buffers.current.size() + lineSize));
        }
        buffers.current.append(line);
        buffers.current.push_back(kNewLine);

        // Hand the buffer over as soon as it is full: the line filled it exactly, or it is a single line that is
        // longer than the buffer size and goes out on its own. If the thread is at its limit of waiting buffers,
        // the buffer stays here until the next line needs the space or the sender collects it.
        if (buffers.current.size() >= buffers.bufferSize && !buffers.waiting.Full())
        {
            HandOver(buffers);
            handedOver = true;
        }
    }

    if (handedOver)
    {
        buffers.owner->SignalSender();
    }
    return true;
}

void BufferedWriter::HandOver(ThreadBuffers& buffers) noexcept
{
    buffers.waiting.Push(std::move(buffers.current));
    buffers.current.clear();  // a moved-from string is valid, but not necessarily empty
}

// FullBufferPolicy::Send: the calling thread sends its own oldest waiting buffer. Called and left with the thread's
// own lock held. This is the only place where a recording thread sends.
void BufferedWriter::SendOldest(ThreadBuffers& buffers, std::unique_lock<std::mutex>& lock)
{
    // The socket lock comes before a thread's own lock.
    lock.unlock();
    std::unique_lock<std::mutex> socketLock(m_socketMutex);
    lock.lock();
    if (buffers.stopped || !buffers.waiting.Full())
    {
        return;  // shut down, or the sender has made room in the meantime
    }

    // Taken and sent under the socket lock, like the sender does, so the buffers of this thread stay in order.
    const std::string oldest = buffers.waiting.Pop();
    lock.unlock();
    Send(oldest);
    socketLock.unlock();
    lock.lock();
}

// Tells the sender that there is something to take: a full buffer, or the buffers of a thread that has exited.
//
// The sender must not go to sleep and miss this, and recording threads should not share a lock to say it. So the
// sender clears m_workPending before each pass over the threads, and before it sleeps it sets m_senderWaiting and
// then checks m_workPending, all while holding m_wakeMutex. A signalling thread sets m_workPending and then checks
// m_senderWaiting. With sequentially consistent operations, at least one of the two sees the other's flag: either
// the sender finds work pending and makes another pass, or this thread finds the sender waiting and wakes it up.
// Only the one thread that wakes a sleeping sender takes m_wakeMutex; while the sender is busy, signalling threads
// only read the two flags.
void BufferedWriter::SignalSender()
{
    if (!m_workPending.load())
    {
        m_workPending.store(true);
    }
    if (m_senderWaiting.load() && m_senderWaiting.exchange(false))
    {
        {
            // The sender holds this lock from before it sets m_senderWaiting until it waits, so after this it is
            // waiting (or already awake again) and the notification cannot be lost.
            std::lock_guard<std::mutex> lock(m_wakeMutex);
        }
        m_wakeCv.notify_one();
    }
}

void BufferedWriter::RecordSlow(void* value, const std::string& line)
{
    // A thread that has let go of its buffers at exit: the key holds the mark, or else the flag is set.
    if (value != nullptr ? HasNoBuffers(value) : t_threadExited)
    {
        RecordAfterThreadExit(line);
        return;
    }

    if (auto* const buffers = static_cast<ThreadBuffers*>(value); buffers != nullptr)
    {
        if (buffers->owner.get() == this)
        {
            return;  // Record() turned the line down: this writer has shut down, and the line is dropped
        }
        // The buffers belong to an earlier initialization. That writer has shut down and sent what was in them.
        pthread_setspecific(g_current.threadKey, nullptr);
        Detach(buffers);
    }

    if (m_shutdown.load())
    {
        return;
    }
    Record(*Attach(), line);
}

// A line recorded on a thread that has already let go of its buffers at exit. It gets buffers of its own that are
// closed from the start, so only the sender sends the line, and then releases them. They are registered after the
// buffers that the thread had before, and the sender empties closed buffers completely in registration order, so
// the line still follows the thread's earlier lines. The limit of waiting buffers and the full-buffer policy do
// not apply to such a line.
void BufferedWriter::RecordAfterThreadExit(const std::string& line)
{
    auto buffers = std::make_shared<ThreadBuffers>(shared_from_this(), m_bufferSize, m_policy);
    buffers->current.reserve(line.size() + 1);
    buffers->current.append(line);
    buffers->current.push_back(kNewLine);
    buffers->closed = true;
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        if (m_shutdown.load())
        {
            return;
        }
        m_registry.push_back(std::move(buffers));
        m_registryChanged = true;
    }
    SignalSender();
}

// Creates and registers buffers for the calling thread, and attaches them to it.
ThreadBuffers* BufferedWriter::Attach()
{
    auto buffers = std::make_shared<ThreadBuffers>(shared_from_this(), m_bufferSize, m_policy);
    ThreadBuffers* const attached = buffers.get();
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        // Shutdown() sets m_shutdown before it stops the registered buffers under this lock, so buffers that it
        // does not find in the registry see the flag here.
        attached->stopped = m_shutdown.load();
        m_registry.push_back(std::move(buffers));
        m_registryChanged = true;
    }

    if (pthread_setspecific(g_current.threadKey, attached) != 0)
    {
        // Out of memory. The thread cannot keep the buffers, so let go of them again.
        Detach(attached);
        throw std::bad_alloc();
    }
    return attached;
}

void BufferedWriter::Detach(ThreadBuffers* buffers)
{
    // Both have to outlive this call: the writer, although these buffers may be the last thing that refers to it,
    // and the buffers, although the sender may release them as soon as they are closed. The writer is declared
    // first, so it is destroyed last.
    const std::shared_ptr<BufferedWriter> writer = buffers->owner;
    const std::shared_ptr<ThreadBuffers> self = buffers->shared_from_this();
    {
        std::lock_guard<std::mutex> lock(buffers->mutex);
        buffers->closed = true;
    }

    bool released = false;
    {
        std::lock_guard<std::mutex> lock(writer->m_registryMutex);
        if (writer->m_finished)
        {
            // The writer has shut down and done its final drain, so nothing is left in the buffers and nobody else
            // will release them.
            released = std::erase(writer->m_registry, self) > 0;
        }
    }
    if (!released)
    {
        writer->SignalSender();
    }
}

void BufferedWriter::SenderLoop()
{
    Clock::time_point nextFlush = Clock::now() + m_flushInterval;
    while (!m_shutdown.load())
    {
        // Cleared before the pass, so that a signal that arrives during the pass is still pending after it.
        m_workPending.store(false);

        const Clock::time_point now = Clock::now();
        const bool flush = now >= nextFlush;
        if (flush)
        {
            nextFlush = now + m_flushInterval;
        }

        const bool sent = Pass(flush);
        if (flush)
        {
            ReportDropped();
        }
        if (sent)
        {
            continue;  // the pass is limited per thread, so more may be waiting
        }

        std::unique_lock<std::mutex> lock(m_wakeMutex);
        m_senderWaiting.store(true);
        if (!m_workPending.load() && !m_shutdown.load())
        {
            m_wakeCv.wait_until(lock, nextFlush);
        }
        m_senderWaiting.store(false);
    }
}

// Visits every thread once and sends what it has waiting. A flush pass also takes the buffers that are still being
// filled. Returns whether anything was sent.
bool BufferedWriter::Pass(bool flush)
{
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        if (m_registryChanged)
        {
            m_visiting.clear();
            for (const auto& buffers : m_registry)
            {
                m_visiting.push_back(buffers.get());
            }
            m_registryChanged = false;
        }
    }

    bool sent = false;
    std::size_t index = 0;
    while (index < m_visiting.size() && !m_shutdown.load())
    {
        const Visited visited = Visit(*m_visiting[index], flush);
        sent = sent || visited.sent;
        if (visited.finished)
        {
            Release(index);
        }
        else
        {
            ++index;
        }
    }
    return sent;
}

// Sends the waiting buffers of one thread, oldest first, and after them the buffer that is still being filled if
// `takePartial` is set or the thread has gone.
BufferedWriter::Visited BufferedWriter::Visit(ThreadBuffers& buffers, bool takePartial)
{
    Visited visited;
    // At most a full waiting list and the buffer being filled per visit, so that a thread that records without
    // pause cannot keep the sender from the other threads. This is enough to empty the buffers of a thread that
    // has gone.
    for (std::size_t taken = 0; taken <= kMaxWaitingBuffers; ++taken)
    {
        std::string buffer;
        bool wakeOwner = false;
        bool nothingOlderLeft = false;

        // A buffer is taken and sent under the socket lock. That keeps the buffers of a thread in order when the
        // thread sends one itself under FullBufferPolicy::Send.
        std::lock_guard<std::mutex> socketLock(m_socketMutex);
        {
            std::lock_guard<std::mutex> lock(buffers.mutex);
            m_dropped += std::exchange(buffers.dropped, 0);
            if (!buffers.waiting.Empty())
            {
                buffer = buffers.waiting.Pop();
                wakeOwner = buffers.ownerWaiting;
            }
            else
            {
                // The buffer being filled is taken only once nothing is waiting, so its lines cannot overtake the
                // thread's full buffers.
                if ((takePartial || buffers.closed) && !buffers.current.empty())
                {
                    buffer.swap(buffers.current);
                }
                nothingOlderLeft = true;
                // If the thread has gone, nothing will be added either.
                visited.finished = buffers.closed;
            }
        }
        if (wakeOwner)
        {
            buffers.room.notify_one();
        }
        if (!buffer.empty())
        {
            Send(buffer);
            visited.sent = true;
        }
        if (nothingOlderLeft)
        {
            break;
        }
    }
    return visited;
}

// Releases the buffers of a thread that has gone, once everything in them has been sent.
void BufferedWriter::Release(std::size_t index)
{
    const ThreadBuffers* const buffers = m_visiting[index];
    m_visiting.erase(m_visiting.begin() + static_cast<std::ptrdiff_t>(index));

    std::shared_ptr<ThreadBuffers> released;
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        const auto it = std::find_if(m_registry.begin(), m_registry.end(),
                                     [buffers](const auto& registered) { return registered.get() == buffers; });
        if (it != m_registry.end())
        {
            released = std::move(*it);
            m_registry.erase(it);
        }
    }
}

void BufferedWriter::ReportDropped()
{
    if (m_dropped == 0)
    {
        return;
    }
    Logger::warn("Buffered writer: dropped {} lines, because the buffers of the recording threads were full",
                 m_dropped);
    m_dropped = 0;
}

// The caller holds m_socketMutex.
void BufferedWriter::Send(const std::string& buffer)
{
    if (m_writer == nullptr)
    {
        return;  // shut down
    }
    try
    {
        m_writer->Write(buffer);
    }
    catch (const std::exception& e)
    {
        Logger::error("Buffered writer: failed to send {} bytes: {}", buffer.size(), e.what());
    }
}

void BufferedWriter::Shutdown()
{
    std::lock_guard<std::mutex> shutdownLock(m_shutdownMutex);
    if (m_shutdownDone)
    {
        return;
    }
    m_shutdownDone = true;

    // 1. No more lines. Lines recorded from here on are dropped, and threads that are waiting for buffer space
    //    are released. Buffers that are registered later are stopped by Attach().
    m_shutdown.store(true);
    {
        std::lock_guard<std::mutex> registryLock(m_registryMutex);
        for (const auto& buffers : m_registry)
        {
            {
                std::lock_guard<std::mutex> lock(buffers->mutex);
                buffers->stopped = true;
            }
            buffers->room.notify_all();
        }
    }

    // 2. Stop the sender thread. It finishes the buffer that it is sending.
    {
        // Taken so that the sender is either before its check of m_shutdown or already waiting.
        std::lock_guard<std::mutex> lock(m_wakeMutex);
    }
    m_wakeCv.notify_all();
    if (m_sender.joinable())
    {
        m_sender.join();
    }

    // 3. Send what is left on this thread: the buffers of threads that have exited and that the sender did not get
    //    to, and the buffers of threads that are still running.
    DrainAll();

    // 4. Nothing is sent after this, so the owner of the socket writer may close and destroy it. A thread that
    //    still gets the socket lock finds its buffers stopped.
    std::lock_guard<std::mutex> socketLock(m_socketMutex);
    m_writer = nullptr;
}

// The final drain, on the thread that shuts the writer down, after the sender thread has been joined.
void BufferedWriter::DrainAll()
{
    std::vector<std::shared_ptr<ThreadBuffers>> all;
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        all = m_registry;
    }
    for (const auto& buffers : all)
    {
        // The buffers are stopped, so nothing is added to them any more.
        while (Visit(*buffers, true).sent)
        {
        }
    }
    ReportDropped();

    // Release the buffers of threads that have gone. The buffers of threads that are still running stay registered,
    // and with them this writer stays alive, until those threads let go of them.
    std::vector<std::shared_ptr<ThreadBuffers>> released;
    {
        std::lock_guard<std::mutex> registryLock(m_registryMutex);
        for (auto it = m_registry.begin(); it != m_registry.end();)
        {
            bool closed = false;
            {
                std::lock_guard<std::mutex> lock((*it)->mutex);
                closed = (*it)->closed;
            }
            if (closed)
            {
                released.push_back(std::move(*it));
                it = m_registry.erase(it);
            }
            else
            {
                ++it;
            }
        }
        m_finished = true;
    }
}

// The destructor of the thread key: the calling thread is exiting. It lets go of the thread's buffers and sets
// t_threadExited. Where the thread is also marked in the key (kMarkExitedThreadInKey), it leaves the mark in the
// place of the buffers; elsewhere the key stays empty and this destructor does not run again.
//
// Setting the key again makes this destructor run again in the next round of key destructors, now with the mark as
// `value`. The mark is renewed each time, so that it is still there for destructors of other keys in later rounds,
// but only kMaxExitedRounds times: implementations are allowed to run rounds for as long as values are set.
void OnThreadExit(void* value)
{
    if (HasNoBuffers(value))
    {
        // A later round, and `value` is the mark.
        const auto rounds = reinterpret_cast<std::uintptr_t>(value);
        if (rounds < kMaxExitedRounds)
        {
            pthread_setspecific(g_current.threadKey, reinterpret_cast<void*>(rounds + 1));
        }
        return;
    }

    BufferedWriter::Detach(static_cast<ThreadBuffers*>(value));
    if (kMarkExitedThreadInKey)
    {
        pthread_setspecific(g_current.threadKey, reinterpret_cast<void*>(std::uintptr_t{1}));
    }
    t_threadExited = true;
}

}  // namespace

namespace buffered_writer {

void Start(BaseWriter* socketWriter, std::size_t bufferSize, std::chrono::milliseconds flushInterval,
           FullBufferPolicy fullBufferPolicy)
{
    if (!g_current.threadKeyCreated)
    {
        if (const int error = pthread_key_create(&g_current.threadKey, &OnThreadExit); error != 0)
        {
            throw std::system_error(error, std::generic_category(), "Could not create the key for thread buffers");
        }
        g_current.threadKeyCreated = true;
    }

    auto buffered = std::make_shared<BufferedWriter>(socketWriter, bufferSize, flushInterval, fullBufferPolicy);
    buffered->Start();
    CurrentOwner() = buffered;
    g_current.writer.store(buffered.get());
}

void Stop()
{
    // Threads that still have buffers of this writer find them stopped on their next line. They get new buffers
    // if buffering is turned on again, and otherwise let go of the old ones when they exit.
    g_current.writer.store(nullptr);
    if (const std::shared_ptr<BufferedWriter> previous = std::move(CurrentOwner()); previous != nullptr)
    {
        previous->Shutdown();
    }
}

void ShutdownAtExit()
{
    if (const std::shared_ptr<BufferedWriter>& buffered = CurrentOwner(); buffered != nullptr)
    {
        buffered->Shutdown();
    }
}

bool Record(const std::string& line)
{
    BufferedWriter* const buffered = g_current.writer.load(std::memory_order_acquire);
    if (buffered == nullptr)
    {
        return false;
    }

    // Normal operation: only the calling thread's own buffers are touched.
    void* const value = pthread_getspecific(g_current.threadKey);
    if (HasNoBuffers(value) || !BufferedWriter::Record(*static_cast<ThreadBuffers*>(value), line))
    {
        // The first line of this thread, a thread that has exited, or buffers of a writer that has shut down.
        buffered->RecordSlow(value, line);
    }
    return true;
}

}  // namespace buffered_writer

}  // namespace spectator
