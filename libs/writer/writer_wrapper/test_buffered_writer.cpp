#include <buffered_writer.h>

#include <logger.h>

#include <gtest/gtest.h>
#include <fmt/core.h>
#include <spdlog/sinks/base_sink.h>

#include <pthread.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace spectator;

// These tests use the buffered writer directly, through buffered_writer.h, with a FakeSocketWriter in place of a
// socket. The fake keeps what is sent and can hold a send back, which stalls the sender thread like a socket whose
// receiver does not read. With the sender stalled, a test brings a thread to its limit of waiting buffers step by
// step, so what happens at that limit does not depend on which thread is faster.

namespace {

// How long a test waits for something that has to happen before it gives up and fails.
constexpr std::chrono::seconds kTimeout{30};

// How long a test watches for something that must not happen.
constexpr std::chrono::milliseconds kQuietTime{100};

// A flush interval that is longer than any test runs, for the tests in which nothing may be sent on a timer.
constexpr std::chrono::milliseconds kNoTimedFlush = std::chrono::hours{1};

// The buffer size at which every Line() fills a buffer exactly, so that each is handed over as it is recorded.
constexpr std::size_t kOneLine = 8;

// The number of full buffers that a thread may have waiting for the sender: kMaxWaitingBuffers in
// buffered_writer.cpp.
constexpr int kMaxWaiting = 8;

using Datagrams = std::vector<std::string>;

// A line that is 8 bytes long with its newline: a letter and a six-digit number.
std::string Line(char letter, int number) { return fmt::format("{}{:06d}", letter, number); }

// The datagram that holds just the given line.
std::string Alone(const std::string& line) { return line + '\n'; }

// The datagrams that hold Line(letter, first) to Line(letter, last), one line each.
Datagrams EachAlone(char letter, int first, int last)
{
    Datagrams datagrams;
    for (int number = first; number <= last; ++number)
    {
        datagrams.push_back(Alone(Line(letter, number)));
    }
    return datagrams;
}

// Stands in for the socket writer. It keeps every datagram together with the thread that sent it. Its sends can be
// held back: a Write() that is held blocks, like a send to a socket whose receiver does not read.
class FakeSocketWriter final : public BaseWriter
{
   public:
    struct Datagram
    {
        std::string data;
        std::thread::id sentBy;
    };

    void Write(const std::string& message) override
    {
        // The real socket writers are not thread-safe, so no two calls may overlap.
        const CallInProgress callInProgress(*this);

        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_failNextWrite)
        {
            m_failNextWrite = false;
            ++m_failedWrites;
            throw std::runtime_error("FakeSocketWriter: this send fails");
        }
        if (m_holding && m_allowed == 0)
        {
            m_held = message;
            m_changed.notify_all();
            m_changed.wait(lock, [this] { return !m_holding || m_allowed > 0; });
            m_held.reset();
        }
        if (m_holding)
        {
            --m_allowed;
        }
        m_lines += static_cast<std::size_t>(std::count(message.begin(), message.end(), '\n'));
        m_datagrams.push_back({message, std::this_thread::get_id()});
        m_changed.notify_all();
    }

    void Close() override {}

    // From now on, a Write() does not return until it is allowed to.
    void HoldWrites()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_holding = true;
        m_allowed = 0;
    }

    // Lets `count` more calls of Write() return.
    void AllowWrites(std::size_t count)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_allowed += count;
            m_held.reset();
        }
        m_changed.notify_all();
    }

    // Stops holding: every Write() returns.
    void ReleaseWrites()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_holding = false;
            m_held.reset();
        }
        m_changed.notify_all();
    }

    // The next Write() throws, like a send that fails.
    void FailNextWrite()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failNextWrite = true;
    }

    // Waits until a Write() is being held and returns its datagram, or nothing if none is held in time.
    std::optional<std::string> WaitForHeldWrite()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_changed.wait_for(lock, kTimeout, [this] { return m_held.has_value(); });
        return m_held;
    }

    // Waits until at least `count` datagrams have been sent. False if that does not happen in time.
    bool WaitForDatagrams(std::size_t count)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_changed.wait_for(lock, kTimeout, [this, count] { return m_datagrams.size() >= count; });
    }

    // Waits until the datagrams that have been sent hold at least `count` lines. False if that does not happen in
    // time.
    bool WaitForLines(std::size_t count)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_changed.wait_for(lock, kTimeout, [this, count] { return m_lines >= count; });
    }

    // The datagrams sent so far, in the order in which they were sent.
    std::vector<Datagram> SentDatagrams() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_datagrams;
    }

    // The same, without the threads that sent them.
    Datagrams Sent() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Datagrams sent;
        for (const Datagram& datagram : m_datagrams)
        {
            sent.push_back(datagram.data);
        }
        return sent;
    }

    std::size_t FailedWrites() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_failedWrites;
    }

    // Whether a Write() ever began while another one was in progress.
    bool Overlapped() const { return m_overlapped.load(); }

    // Forgets the datagrams sent so far.
    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_datagrams.clear();
        m_lines = 0;
    }

   private:
    class CallInProgress
    {
       public:
        explicit CallInProgress(FakeSocketWriter& writer) : m_writer(writer)
        {
            if (m_writer.m_callsInProgress.fetch_add(1) != 0)
            {
                m_writer.m_overlapped.store(true);
            }
        }
        ~CallInProgress() { m_writer.m_callsInProgress.fetch_sub(1); }

        CallInProgress(const CallInProgress&) = delete;
        CallInProgress& operator=(const CallInProgress&) = delete;

       private:
        FakeSocketWriter& m_writer;
    };

    std::atomic<int> m_callsInProgress{0};
    std::atomic<bool> m_overlapped{false};

    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::vector<Datagram> m_datagrams;
    std::size_t m_lines = 0;
    bool m_holding = false;
    std::size_t m_allowed = 0;          // while holding: the number of calls that may still return
    std::optional<std::string> m_held;  // the datagram of the call that is being held
    bool m_failNextWrite = false;
    std::size_t m_failedWrites = 0;
};

// A thread that records the lines it is told to and stays alive in between. A test uses it to decide what a thread
// other than its own has recorded, and when that thread exits.
class RecordingThread
{
   public:
    RecordingThread() : m_thread([this] { Run(); }) {}

    ~RecordingThread()
    {
        Exit();
        m_gone.wait();
    }

    RecordingThread(const RecordingThread&) = delete;
    RecordingThread& operator=(const RecordingThread&) = delete;

    std::thread::id Id() const { return m_id; }

    // Starts `work` on the thread and returns at once. The result is there once the thread has done the work.
    std::future<bool> StartWork(std::function<bool()> work)
    {
        std::packaged_task<bool()> task(std::move(work));
        std::future<bool> result = task.get_future();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_tasks.push_back(std::move(task));
        }
        m_wake.notify_one();
        return result;
    }

    // Does `work` on the thread. False if the work returns false, or does not return in time.
    bool Do(std::function<bool()> work)
    {
        std::future<bool> result = StartWork(std::move(work));
        return result.wait_for(kTimeout) == std::future_status::ready && result.get();
    }

    // Starts recording `line` on the thread and returns at once, for a line that the thread may have to wait with.
    std::future<bool> StartRecording(const std::string& line)
    {
        return StartWork([line] { return buffered_writer::Record(line); });
    }

    // Records `line` on the thread. False if the line is not taken, or the thread is still waiting with it when
    // time is up.
    bool Record(const std::string& line)
    {
        return Do([line] { return buffered_writer::Record(line); });
    }

    // Ends the thread, which makes it let go of its buffers, and waits until it is gone. False if it is not gone in
    // time.
    bool Exit()
    {
        if (!m_gone.valid())
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_exit = true;
            }
            m_wake.notify_one();
            // Joined on a thread of its own, so that a thread that does not exit fails the test instead of hanging
            // it.
            m_gone = std::async(std::launch::async, [this] { m_thread.join(); });
        }
        return m_gone.wait_for(kTimeout) == std::future_status::ready;
    }

   private:
    void Run()
    {
        for (;;)
        {
            std::packaged_task<bool()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_wake.wait(lock, [this] { return m_exit || !m_tasks.empty(); });
                if (m_tasks.empty())
                {
                    return;
                }
                task = std::move(m_tasks.front());
                m_tasks.pop_front();
            }
            task();
        }
    }

    // Used by Run(), so they come before m_thread, which starts it.
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<std::packaged_task<bool()>> m_tasks;
    bool m_exit = false;

    std::thread m_thread;
    const std::thread::id m_id{m_thread.get_id()};
    std::future<void> m_gone;
};

// Keeps what the library logs. The number of lines that a thread has dropped is only reported in the log, as
// "Buffered writer: dropped <count> lines, ...", so this is where a test reads it.
class LogCapture final : public spdlog::sinks::base_sink<std::mutex>
{
   public:
    // Forgets what has been logged up to now. False if the logger does not respond in time.
    bool StartOver()
    {
        const bool arrived = WaitForLogger();
        const std::lock_guard<std::mutex> lock(mutex_);
        m_messages.clear();
        return arrived;
    }

    // The number of lines that the buffered writer has reported as dropped since StartOver().
    std::uint64_t DroppedLines()
    {
        static constexpr std::string_view kReport = "Buffered writer: dropped ";

        WaitForLogger();
        const std::lock_guard<std::mutex> lock(mutex_);
        std::uint64_t dropped = 0;
        for (const std::string& message : m_messages)
        {
            if (message.starts_with(kReport))
            {
                dropped += std::stoull(message.substr(kReport.size()));
            }
        }
        return dropped;
    }

   protected:
    void sink_it_(const spdlog::details::log_msg& message) override
    {
        m_messages.emplace_back(message.payload.data(), message.payload.size());
    }

    void flush_() override
    {
        ++m_flushes;
        m_flushed.notify_all();
    }

   private:
    // The logger hands its messages to a thread of its own, which passes them on to the sinks. A flush goes the same
    // way, so once it has got here, so has everything that was logged before it.
    bool WaitForLogger()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        const std::uint64_t flushesBefore = m_flushes;
        lock.unlock();
        Logger::GetLogger()->flush();
        lock.lock();
        return m_flushed.wait_for(lock, kTimeout, [this, flushesBefore] { return m_flushes > flushesBefore; });
    }

    // Guarded by mutex_ of the base class, which is held whenever sink_it_() and flush_() are called.
    std::vector<std::string> m_messages;
    std::uint64_t m_flushes = 0;
    std::condition_variable m_flushed;
};

// The capture, which is added to the library's logger on the first call.
LogCapture& Log()
{
    static const std::shared_ptr<LogCapture> capture = []
    {
        auto created = std::make_shared<LogCapture>();
        Logger::GetLogger()->sinks().push_back(created);
        return created;
    }();
    return *capture;
}

// Adds the capture before the first test. Nothing has been logged at that point, so the logger's thread is not
// reading the list of sinks while it changes.
class LogCaptureEnvironment : public testing::Environment
{
   public:
    void SetUp() override { Log(); }
};

testing::Environment* const logCaptureEnvironment = testing::AddGlobalTestEnvironment(new LogCaptureEnvironment);

// The line with the given number that the given thread records in the tests with several threads. The lines of a
// thread differ in length.
std::string ThreadLine(int thread, int number)
{
    return fmt::format("t{}.{}.{}", thread, number, std::string(static_cast<std::size_t>(number % 40), 'x'));
}

// What arrived of the lines that threads recorded with ThreadLine(), and what was wrong with it.
struct Arrived
{
    std::vector<int> lines;  // per thread, the number of its lines that arrived
    int total = 0;           // the same for all threads together
    int tooLarge = 0;        // datagrams that are larger than the buffer size
    int notWholeLines = 0;   // datagrams that do not end with a newline, and lines that are not as they were recorded
    int mixed = 0;           // lines in a datagram that begins with a line of another thread
    int outOfOrder = 0;      // lines that arrived after a later line of their thread, or twice
};

Arrived Examine(const Datagrams& datagrams, std::size_t bufferSize, int threads, int linesPerThread)
{
    Arrived arrived;
    arrived.lines.assign(static_cast<std::size_t>(threads), 0);
    std::vector<int> next(static_cast<std::size_t>(threads), 0);  // per thread, the lowest number its next line has

    for (const std::string& datagram : datagrams)
    {
        if (datagram.size() > bufferSize)
        {
            ++arrived.tooLarge;
        }
        if (datagram.empty() || datagram.back() != '\n')
        {
            ++arrived.notWholeLines;
            continue;
        }

        int datagramThread = -1;
        for (std::size_t start = 0; start < datagram.size();)
        {
            const std::size_t end = datagram.find('\n', start);
            const std::string line = datagram.substr(start, end - start);
            start = end + 1;

            int thread = -1;
            int number = -1;
            if (std::sscanf(line.c_str(), "t%d.%d.", &thread, &number) != 2 || thread < 0 || thread >= threads ||
                number < 0 || number >= linesPerThread || line != ThreadLine(thread, number))
            {
                ++arrived.notWholeLines;
                continue;
            }

            if (datagramThread == -1)
            {
                datagramThread = thread;
            }
            else if (thread != datagramThread)
            {
                ++arrived.mixed;
            }

            const auto index = static_cast<std::size_t>(thread);
            if (number < next[index])
            {
                ++arrived.outOfOrder;
            }
            else
            {
                next[index] = number + 1;
            }
            ++arrived.lines[index];
            ++arrived.total;
        }
    }
    return arrived;
}

// Stands in for code that still records while a thread is being torn down: the destructor of a pthread key, which
// runs once in every round of key destructors for as long as it sets its key again. The buffered writer lets go of
// the thread's buffers in the first round, in the destructor of its own key, so from the second round on these lines
// are recorded on a thread that has no buffers any more.
//   Round 1: one line. Depending on which of the two destructors runs first, the thread still has its buffers.
//   Round 2: more lines than a thread may have waiting, and may hold at all.
//   Round 4, the last round that an implementation has to run: a line that does not fill a buffer.
constexpr int kLastLateLineOfRound2 = kMaxWaiting + 4;
constexpr const char* kLineOfLastRound = "end";

pthread_key_t LateRecorderKey();

void RecordLate(void* value)
{
    int& round = *static_cast<int*>(value);
    ++round;
    if (round == 1)
    {
        buffered_writer::Record(Line('z', 1));
    }
    else if (round == 2)
    {
        for (int number = 2; number <= kLastLateLineOfRound2; ++number)
        {
            buffered_writer::Record(Line('z', number));
        }
    }
    else if (round == 4)
    {
        buffered_writer::Record(kLineOfLastRound);
    }
    if (round < 4)
    {
        pthread_setspecific(LateRecorderKey(), value);  // makes this destructor run again in the next round
    }
}

pthread_key_t LateRecorderKey()
{
    static const pthread_key_t key = []
    {
        pthread_key_t created{};
        if (pthread_key_create(&created, &RecordLate) != 0)
        {
            throw std::runtime_error("Could not create the key of the late recorder");
        }
        return created;
    }();
    return key;
}

class BufferedWriterTest : public testing::Test
{
   protected:
    void SetUp() override { ASSERT_TRUE(Log().StartOver()); }

    void TearDown() override
    {
        // Whatever a test leaves behind, this lets everything finish: no send is held any longer, and shutting the
        // writer down releases a thread that waits for buffer space.
        m_socket.ReleaseWrites();
        m_secondSocket.ReleaseWrites();
        buffered_writer::Stop();
        for (std::thread& thread : m_threads)
        {
            thread.join();
        }
        m_recordingThreads.clear();

        EXPECT_FALSE(m_socket.Overlapped()) << "the socket writer was used by two threads at once";
        EXPECT_FALSE(m_secondSocket.Overlapped()) << "the socket writer was used by two threads at once";
    }

    // Turns buffering on, with m_socket as the socket writer.
    void StartWriter(std::size_t bufferSize, std::chrono::milliseconds flushInterval = kNoTimedFlush,
                     FullBufferPolicy fullBufferPolicy = FullBufferPolicy::Block)
    {
        buffered_writer::Start(&m_socket, bufferSize, flushInterval, fullBufferPolicy);
    }

    // A thread that records what the test tells it to. It exits when the test tells it to, or else when the test is
    // over.
    RecordingThread& NewThread() { return *m_recordingThreads.emplace_back(std::make_unique<RecordingThread>()); }

    // Runs `work` on a new thread.
    void RunOnNewThread(std::function<void()> work)
    {
        {
            std::lock_guard<std::mutex> lock(m_threadsMutex);
            ++m_threadsAtWork;
        }
        m_threads.emplace_back(
            [this, work = std::move(work)]
            {
                work();
                {
                    std::lock_guard<std::mutex> lock(m_threadsMutex);
                    --m_threadsAtWork;
                }
                m_threadsDone.notify_all();
            });
    }

    // Waits until the threads started with RunOnNewThread() have done their work and are gone. False if they are
    // not done in time, which is how a test fails, instead of hanging, when its threads wait for each other.
    bool WaitForThreads()
    {
        {
            std::unique_lock<std::mutex> lock(m_threadsMutex);
            if (!m_threadsDone.wait_for(lock, kTimeout, [this] { return m_threadsAtWork == 0; }))
            {
                return false;
            }
        }
        for (std::thread& thread : m_threads)
        {
            thread.join();
        }
        m_threads.clear();
        return true;
    }

    // Stalls the sender thread and brings `thread` to its limit, with one line per buffer: the sender is stuck
    // sending the thread's first buffer, eight buffers are waiting, and the tenth is full but still with the thread.
    // The lines are Line(letter, 1) to Line(letter, 10). The writer has to have a buffer size of kOneLine.
    void FillUp(RecordingThread& thread, char letter)
    {
        m_socket.HoldWrites();
        ASSERT_TRUE(thread.Record(Line(letter, 1)));
        // Only once the sender has taken the first buffer is it certain that the next eight stay with the thread.
        ASSERT_EQ(m_socket.WaitForHeldWrite(), Alone(Line(letter, 1)));
        for (int number = 2; number <= kMaxWaiting + 2; ++number)
        {
            ASSERT_TRUE(thread.Record(Line(letter, number)));
        }
    }

    FakeSocketWriter m_socket;
    FakeSocketWriter m_secondSocket;

   private:
    std::vector<std::unique_ptr<RecordingThread>> m_recordingThreads;

    std::mutex m_threadsMutex;
    std::condition_variable m_threadsDone;
    int m_threadsAtWork = 0;
    std::vector<std::thread> m_threads;
};

}  // namespace

// ---------- What goes into a datagram ----------

TEST_F(BufferedWriterTest, SendsABufferWhenTheNextLineDoesNotFit)
{
    StartWriter(16);

    EXPECT_TRUE(buffered_writer::Record("aaaaa"));  // 6 bytes with its newline
    EXPECT_TRUE(buffered_writer::Record("bbbbb"));  // 12 bytes in the buffer
    EXPECT_TRUE(buffered_writer::Record("ccccc"));  // would make 18

    // The first two lines go out without the third, and without a timed flush.
    ASSERT_TRUE(m_socket.WaitForDatagrams(1));
    EXPECT_EQ(m_socket.Sent(), (Datagrams{"aaaaa\nbbbbb\n"}));

    buffered_writer::Stop();
    EXPECT_EQ(m_socket.Sent(), (Datagrams{"aaaaa\nbbbbb\n", "ccccc\n"}));
}

TEST_F(BufferedWriterTest, SendsABufferAsSoonAsALineFillsIt)
{
    StartWriter(12);

    EXPECT_TRUE(buffered_writer::Record("aaaaa"));
    EXPECT_TRUE(buffered_writer::Record("bbbbb"));  // 12 bytes: the buffer is full

    // The buffer goes out without another line being recorded.
    ASSERT_TRUE(m_socket.WaitForDatagrams(1));
    EXPECT_EQ(m_socket.Sent(), (Datagrams{"aaaaa\nbbbbb\n"}));

    buffered_writer::Stop();
    EXPECT_EQ(m_socket.Sent().size(), 1u);
}

TEST_F(BufferedWriterTest, SendsALineLongerThanTheBufferOnItsOwn)
{
    StartWriter(8);
    const std::string longLine(20, 'x');
    const std::string otherLongLine(30, 'y');

    // Into an empty buffer: the line is sent whole, as soon as it is recorded.
    EXPECT_TRUE(buffered_writer::Record(longLine));
    ASSERT_TRUE(m_socket.WaitForDatagrams(1));
    EXPECT_EQ(m_socket.Sent(), (Datagrams{Alone(longLine)}));

    // Into a buffer that holds a line: that line goes out first, and the long line in a datagram of its own.
    EXPECT_TRUE(buffered_writer::Record("ab"));
    EXPECT_TRUE(buffered_writer::Record(otherLongLine));
    ASSERT_TRUE(m_socket.WaitForDatagrams(3));
    EXPECT_EQ(m_socket.Sent(), (Datagrams{Alone(longLine), "ab\n", Alone(otherLongLine)}));

    EXPECT_TRUE(buffered_writer::Record("cd"));
    buffered_writer::Stop();
    EXPECT_EQ(m_socket.Sent(), (Datagrams{Alone(longLine), "ab\n", Alone(otherLongLine), "cd\n"}));
}

// ---------- Turning buffering on and off, and the flush interval ----------

TEST_F(BufferedWriterTest, RecordReturnsFalseWhileBufferingIsOff)
{
    RecordingThread& thread = NewThread();

    // The caller then sends the line itself.
    EXPECT_FALSE(buffered_writer::Record("before"));
    EXPECT_FALSE(thread.Record("before"));

    StartWriter(64);
    EXPECT_TRUE(buffered_writer::Record("this.thread"));
    EXPECT_TRUE(thread.Record("another.thread"));
    buffered_writer::Stop();

    // Both threads still have the buffers of the writer that was turned off.
    EXPECT_FALSE(buffered_writer::Record("after"));
    EXPECT_FALSE(thread.Record("after"));

    EXPECT_EQ(m_socket.Sent(), (Datagrams{"this.thread\n", "another.thread\n"}));
}

TEST_F(BufferedWriterTest, SendsAPartlyFilledBufferAfterTheFlushInterval)
{
    StartWriter(4096, std::chrono::milliseconds(20));

    EXPECT_TRUE(buffered_writer::Record("first"));
    ASSERT_TRUE(m_socket.WaitForDatagrams(1));
    EXPECT_TRUE(buffered_writer::Record("second"));
    ASSERT_TRUE(m_socket.WaitForDatagrams(2));

    EXPECT_EQ(m_socket.Sent(), (Datagrams{"first\n", "second\n"}));
}

TEST_F(BufferedWriterTest, StopSendsWhatEveryThreadStillHolds)
{
    StartWriter(4096);
    RecordingThread& other = NewThread();

    EXPECT_TRUE(buffered_writer::Record("from.this.thread"));
    ASSERT_TRUE(other.Record("from.another.thread"));

    // Neither buffer is full and no timed flush is due, so nothing is sent.
    std::this_thread::sleep_for(kQuietTime);
    EXPECT_TRUE(m_socket.Sent().empty());

    // The other thread is still running. Its line is sent all the same.
    buffered_writer::Stop();
    EXPECT_EQ(m_socket.Sent(), (Datagrams{"from.this.thread\n", "from.another.thread\n"}));
}

TEST_F(BufferedWriterTest, WorksWithAFlushIntervalThatIsOutOfRange)
{
    // Zero or less: the line is still flushed.
    std::size_t sent = 0;
    for (const std::chrono::milliseconds interval : {std::chrono::milliseconds(0), std::chrono::milliseconds(-5)})
    {
        StartWriter(4096, interval);
        EXPECT_TRUE(buffered_writer::Record("line"));
        ASSERT_TRUE(m_socket.WaitForDatagrams(++sent));
        buffered_writer::Stop();
    }

    // The longest interval there is: no timed flush, and not one that is due at once because the time of the next
    // flush has overflowed.
    StartWriter(4096, std::chrono::milliseconds::max());
    EXPECT_TRUE(buffered_writer::Record("line"));
    std::this_thread::sleep_for(kQuietTime);
    EXPECT_EQ(m_socket.Sent().size(), sent);
    buffered_writer::Stop();
    EXPECT_EQ(m_socket.Sent().size(), sent + 1);
}

// ---------- Threads ----------

class BufferedWriterPolicyTest : public BufferedWriterTest, public testing::WithParamInterface<FullBufferPolicy>
{
};

// Several threads record without pause. They produce buffers faster than the one sender thread sends them, so they
// keep reaching their limit of waiting buffers, and what the policy says happens then happens all the time.
TEST_P(BufferedWriterPolicyTest, KeepsEachThreadsLinesInOrder)
{
    constexpr std::size_t kBufferSize = 64;
    constexpr int kThreads = 6;
    constexpr int kLinesPerThread = 20000;
    // A short flush interval, so that timed flushes take partly filled buffers while the threads hand over full
    // ones.
    StartWriter(kBufferSize, std::chrono::milliseconds(1), GetParam());

    for (int thread = 0; thread < kThreads; ++thread)
    {
        RunOnNewThread(
            [thread]
            {
                for (int number = 0; number < kLinesPerThread; ++number)
                {
                    buffered_writer::Record(ThreadLine(thread, number));
                }
            });
    }
    ASSERT_TRUE(WaitForThreads());
    buffered_writer::Stop();

    const Arrived arrived = Examine(m_socket.Sent(), kBufferSize, kThreads, kLinesPerThread);
    EXPECT_EQ(arrived.tooLarge, 0);
    EXPECT_EQ(arrived.notWholeLines, 0);
    EXPECT_EQ(arrived.mixed, 0);
    EXPECT_EQ(arrived.outOfOrder, 0);
    if (GetParam() != FullBufferPolicy::Send)
    {
        // A recording thread sends only under the send policy. Otherwise every datagram comes from the sender
        // thread, or from this thread, which sends what is left when it turns the writer off.
        std::set<std::thread::id> senders;
        for (const FakeSocketWriter::Datagram& datagram : m_socket.SentDatagrams())
        {
            if (datagram.sentBy != std::this_thread::get_id())
            {
                senders.insert(datagram.sentBy);
            }
        }
        EXPECT_LE(senders.size(), 1u);
    }
    if (GetParam() == FullBufferPolicy::Drop)
    {
        // Every line was either sent or counted as dropped.
        EXPECT_EQ(static_cast<std::uint64_t>(arrived.total) + Log().DroppedLines(),
                  static_cast<std::uint64_t>(kThreads * kLinesPerThread));
    }
    else
    {
        for (int thread = 0; thread < kThreads; ++thread)
        {
            EXPECT_EQ(arrived.lines[static_cast<std::size_t>(thread)], kLinesPerThread) << "thread " << thread;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(AllPolicies, BufferedWriterPolicyTest,
                         testing::Values(FullBufferPolicy::Block, FullBufferPolicy::Drop, FullBufferPolicy::Send),
                         [](const testing::TestParamInfo<FullBufferPolicy>& info)
                         { return FullBufferPolicyToString(info.param); });

TEST_F(BufferedWriterTest, SendsWhatAThreadHoldsWhenTheThreadExits)
{
    StartWriter(4096);
    RecordingThread& thread = NewThread();

    ASSERT_TRUE(thread.Record("from.a.thread"));
    EXPECT_TRUE(m_socket.Sent().empty());
    ASSERT_TRUE(thread.Exit());

    // The buffer is not full, no timed flush is due and the writer is still on: the line is sent because the thread
    // has gone.
    ASSERT_TRUE(m_socket.WaitForDatagrams(1));
    EXPECT_EQ(m_socket.Sent(), (Datagrams{"from.a.thread\n"}));
}

TEST_F(BufferedWriterTest, AnExitingThreadNeitherSendsNorWaits)
{
    // The send policy is the one under which a recording thread may send at all.
    StartWriter(kOneLine, kNoTimedFlush, FullBufferPolicy::Send);
    RecordingThread& thread = NewThread();
    const std::thread::id threadId = thread.Id();
    ASSERT_NO_FATAL_FAILURE(FillUp(thread, 'a'));

    // The thread exits while nothing can be sent. It does not wait for that to change, and it sends nothing itself.
    ASSERT_TRUE(thread.Exit());
    EXPECT_TRUE(m_socket.Sent().empty());

    // The sender thread sends what the thread had buffered, in order, including the buffer that was not handed over.
    m_socket.ReleaseWrites();
    ASSERT_TRUE(m_socket.WaitForDatagrams(10));
    EXPECT_EQ(m_socket.Sent(), EachAlone('a', 1, 10));
    for (const FakeSocketWriter::Datagram& datagram : m_socket.SentDatagrams())
    {
        EXPECT_NE(datagram.sentBy, threadId);
    }
}

TEST_F(BufferedWriterTest, KeepsLinesRecordedDuringThreadExitInOrder)
{
    StartWriter(kOneLine, kNoTimedFlush, FullBufferPolicy::Send);
    m_socket.HoldWrites();
    RecordingThread& thread = NewThread();
    const std::thread::id threadId = thread.Id();
    // Static, so that it is still there for RecordLate() if the thread outlives this test.
    static int round;
    round = 0;
    ASSERT_TRUE(thread.Do([] { return pthread_setspecific(LateRecorderKey(), &round) == 0; }));

    // The sender is stuck sending the first line, so the next two wait with the thread.
    ASSERT_TRUE(thread.Record(Line('a', 1)));
    ASSERT_EQ(m_socket.WaitForHeldWrite(), Alone(Line('a', 1)));
    ASSERT_TRUE(thread.Record(Line('a', 2)));
    ASSERT_TRUE(thread.Record(Line('a', 3)));

    // The thread exits, and RecordLate() records more lines on its way out. However many they are, they do not make
    // the thread wait, although nothing can be sent, and the thread does not send them itself.
    ASSERT_TRUE(thread.Exit());
    EXPECT_EQ(round, 4);
    EXPECT_TRUE(m_socket.Sent().empty());

    // The late lines follow the lines that the thread recorded before, in the order in which they were recorded.
    // They are all sent without a timed flush, including the last one, which did not fill a buffer: none is left
    // behind in a buffer that nobody lets go of.
    m_socket.ReleaseWrites();
    Datagrams expected = EachAlone('a', 1, 3);
    const Datagrams late = EachAlone('z', 1, kLastLateLineOfRound2);
    expected.insert(expected.end(), late.begin(), late.end());
    expected.push_back(Alone(kLineOfLastRound));
    ASSERT_TRUE(m_socket.WaitForDatagrams(expected.size()));
    EXPECT_EQ(m_socket.Sent(), expected);
    for (const FakeSocketWriter::Datagram& datagram : m_socket.SentDatagrams())
    {
        EXPECT_NE(datagram.sentBy, threadId);
    }
}

TEST_F(BufferedWriterTest, LosesNothingFromShortLivedThreads)
{
    constexpr std::size_t kBufferSize = 64;
    constexpr int kThreads = 300;
    constexpr int kLinesPerThread = 3;
    constexpr int kThreadsAtATime = 10;
    StartWriter(kBufferSize);

    // A few threads at a time, so that threads come and go while the sender thread is at work.
    for (int first = 0; first < kThreads; first += kThreadsAtATime)
    {
        for (int thread = first; thread < first + kThreadsAtATime; ++thread)
        {
            RunOnNewThread(
                [thread]
                {
                    for (int number = 0; number < kLinesPerThread; ++number)
                    {
                        buffered_writer::Record(ThreadLine(thread, number));
                    }
                });
        }
        ASSERT_TRUE(WaitForThreads());
    }

    // The lines of a thread do not fill a buffer, no timed flush is due and the writer is still on: they are sent
    // because their threads have gone.
    ASSERT_TRUE(m_socket.WaitForLines(static_cast<std::size_t>(kThreads * kLinesPerThread)));
    const Arrived arrived = Examine(m_socket.Sent(), kBufferSize, kThreads, kLinesPerThread);
    EXPECT_EQ(arrived.tooLarge, 0);
    EXPECT_EQ(arrived.notWholeLines, 0);
    EXPECT_EQ(arrived.mixed, 0);
    EXPECT_EQ(arrived.outOfOrder, 0);
    EXPECT_EQ(arrived.total, kThreads * kLinesPerThread);
}

// Ten threads that record lines of exactly the buffer size. An earlier writer could deadlock in this case; see the
// test TenThreadsBufferSize10Messages in test_writer.cpp, which is commented out.
TEST_F(BufferedWriterTest, TenThreadsWithLinesOfExactlyTheBufferSize)
{
    constexpr std::size_t kBufferSize = 21;
    constexpr int kThreads = 10;
    constexpr int kLinesPerThread = 10;
    StartWriter(kBufferSize);

    for (int thread = 0; thread < kThreads; ++thread)
    {
        RunOnNewThread(
            [thread]
            {
                const std::string line = fmt::format("c:ctr{:06d}:1.000000", thread);  // 21 bytes with its newline
                for (int number = 0; number < kLinesPerThread; ++number)
                {
                    buffered_writer::Record(line);
                }
            });
    }
    ASSERT_TRUE(WaitForThreads());

    // Every line fills a buffer, so each is sent as a datagram of its own, without a timed flush.
    ASSERT_TRUE(m_socket.WaitForDatagrams(static_cast<std::size_t>(kThreads * kLinesPerThread)));
    std::map<std::string, int> arrived;
    for (const std::string& datagram : m_socket.Sent())
    {
        EXPECT_EQ(datagram.size(), kBufferSize);
        ++arrived[datagram];
    }
    EXPECT_EQ(arrived.size(), static_cast<std::size_t>(kThreads));
    for (int thread = 0; thread < kThreads; ++thread)
    {
        EXPECT_EQ(arrived[fmt::format("c:ctr{:06d}:1.000000\n", thread)], kLinesPerThread) << "thread " << thread;
    }
}

// ---------- What a thread does when it is out of buffer space ----------

TEST_F(BufferedWriterTest, BlockPolicyMakesAThreadWaitForTheSender)
{
    StartWriter(kOneLine, kNoTimedFlush, FullBufferPolicy::Block);
    RecordingThread& thread = NewThread();
    ASSERT_NO_FATAL_FAILURE(FillUp(thread, 'a'));

    // The eleventh line needs a ninth waiting buffer, so the thread waits for as long as the sender is stuck.
    std::future<bool> eleventh = thread.StartRecording(Line('a', 11));
    EXPECT_EQ(eleventh.wait_for(kQuietTime), std::future_status::timeout);

    // Once the sender takes a buffer, the thread carries on. No line is lost.
    m_socket.ReleaseWrites();
    ASSERT_EQ(eleventh.wait_for(kTimeout), std::future_status::ready);
    EXPECT_TRUE(eleventh.get());
    buffered_writer::Stop();
    EXPECT_EQ(m_socket.Sent(), EachAlone('a', 1, 11));
}

TEST_F(BufferedWriterTest, BlockPolicyThreadIsReleasedByShutdown)
{
    StartWriter(kOneLine, kNoTimedFlush, FullBufferPolicy::Block);
    RecordingThread& thread = NewThread();
    ASSERT_NO_FATAL_FAILURE(FillUp(thread, 'a'));
    std::future<bool> eleventh = thread.StartRecording(Line('a', 11));
    EXPECT_EQ(eleventh.wait_for(kQuietTime), std::future_status::timeout);

    // Shutting down waits for the sender thread, whose send is still held, so it runs on a thread of its own. Until
    // the send is let go below, nothing here may end the test early: the destructor of `stopped` waits for it.
    std::future<void> stopped = std::async(std::launch::async, [] { buffered_writer::Stop(); });

    // The waiting thread is released while the sender is still stuck. Its line is taken, and dropped.
    const bool released = eleventh.wait_for(kTimeout) == std::future_status::ready;
    EXPECT_TRUE(released);
    EXPECT_TRUE(m_socket.Sent().empty());

    m_socket.ReleaseWrites();
    EXPECT_EQ(stopped.wait_for(kTimeout), std::future_status::ready);
    if (released)
    {
        EXPECT_TRUE(eleventh.get());
    }
    EXPECT_EQ(m_socket.Sent(), EachAlone('a', 1, 10));
}

TEST_F(BufferedWriterTest, DropPolicyDropsNewLinesAndKeepsBufferedOnes)
{
    StartWriter(kOneLine, kNoTimedFlush, FullBufferPolicy::Drop);
    RecordingThread& thread = NewThread();
    ASSERT_NO_FATAL_FAILURE(FillUp(thread, 'a'));

    // The thread is out of buffer space and the sender is stuck: three more lines are dropped, and the thread does
    // not wait.
    for (int number = 11; number <= 13; ++number)
    {
        ASSERT_TRUE(thread.Record(Line('a', number)));
    }

    // The ten lines that were buffered are sent, and the number of dropped lines is logged.
    m_socket.ReleaseWrites();
    buffered_writer::Stop();
    EXPECT_EQ(m_socket.Sent(), EachAlone('a', 1, 10));
    EXPECT_EQ(Log().DroppedLines(), 3u);
}

// Under the send policy, a thread that needs a ninth waiting buffer sends its oldest one itself. With one thread and
// ten lines, that only happens if the thread has recorded all ten before the sender thread takes the first buffer,
// and a test cannot make the sender thread be late. What it can do is record right after the writer is started, when
// the new sender thread is not running yet, and start over with a new writer if the sender thread was in time after
// all. (With several threads that record without pause, as in KeepsEachThreadsLinesInOrder, threads send for
// themselves all the time. This test is about one such send: which buffer it is, and where it ends up.)
TEST_F(BufferedWriterTest, SendPolicyLetsAThreadSendItsOwnOldestBuffer)
{
    constexpr int kAttempts = 1000;
    // Ten lines of one buffer each: eight buffers wait, the ninth is still with the thread, and the tenth line needs
    // a ninth waiting buffer.
    Datagrams lines;
    for (int number = 1; number <= kMaxWaiting + 2; ++number)
    {
        lines.push_back(Line('a', number));
    }
    const std::thread::id thisThread = std::this_thread::get_id();

    bool sentByThisThread = false;
    for (int attempt = 0; attempt < kAttempts && !sentByThisThread; ++attempt)
    {
        m_socket.Clear();
        StartWriter(kOneLine, kNoTimedFlush, FullBufferPolicy::Send);
        for (const std::string& line : lines)
        {
            buffered_writer::Record(line);
        }
        // What this thread has sent by now, it has sent while recording. After this, it also sends what is left
        // when it turns the writer off.
        const std::vector<FakeSocketWriter::Datagram> sentWhileRecording = m_socket.SentDatagrams();
        buffered_writer::Stop();

        for (const FakeSocketWriter::Datagram& datagram : sentWhileRecording)
        {
            if (datagram.sentBy == thisThread)
            {
                sentByThisThread = true;
                // It is the oldest buffer, and the first datagram to be sent at all.
                EXPECT_EQ(datagram.data, Alone(lines.front()));
                EXPECT_EQ(sentWhileRecording.front().sentBy, thisThread);
            }
        }
        // Whoever sent which buffer, every line arrives once, and in order.
        ASSERT_EQ(m_socket.Sent(), EachAlone('a', 1, kMaxWaiting + 2));
    }
    EXPECT_TRUE(sentByThisThread) << "the sender thread took a buffer first in each of " << kAttempts << " attempts";
}

// ---------- The sender thread, and the life of the writer ----------

// A thread that records without pause always has buffers waiting. The sender thread takes at most nine buffers of a
// thread before it goes on to the next thread, so that such a thread does not keep it from the others.
TEST_F(BufferedWriterTest, SenderLeavesABusyThreadAfterNineBuffers)
{
    StartWriter(kOneLine);
    m_socket.HoldWrites();
    RecordingThread& other = NewThread();
    RecordingThread& busy = NewThread();

    // The other thread records first, so the sender thread visits it first in each of its rounds.
    ASSERT_TRUE(other.Record(Line('b', 0)));
    ASSERT_EQ(m_socket.WaitForHeldWrite(), Alone(Line('b', 0)));

    // The busy thread fills up: eight buffers wait, and the ninth is still with it.
    for (int number = 1; number <= kMaxWaiting + 1; ++number)
    {
        ASSERT_TRUE(busy.Record(Line('a', number)));
    }
    // The sender gets on to the busy thread. Then the other thread has something to send again.
    m_socket.AllowWrites(1);
    ASSERT_EQ(m_socket.WaitForHeldWrite(), Alone(Line('a', 1)));
    ASSERT_TRUE(other.Record(Line('b', 1)));

    // Each time the sender has taken a buffer of the busy thread, that thread hands over another one, so it never
    // runs out of waiting buffers.
    for (int taken = 1; taken <= kMaxWaiting + 1; ++taken)
    {
        ASSERT_EQ(m_socket.WaitForHeldWrite(), Alone(Line('a', taken)));
        ASSERT_TRUE(busy.Record(Line('a', kMaxWaiting + 1 + taken)));
        m_socket.AllowWrites(1);
    }

    // After nine buffers, the sender leaves the busy thread, which has eight more waiting, for the other thread.
    ASSERT_EQ(m_socket.WaitForHeldWrite(), Alone(Line('b', 1)));

    m_socket.ReleaseWrites();
    buffered_writer::Stop();
    Datagrams expected{Alone(Line('b', 0))};
    for (int number = 1; number <= kMaxWaiting + 1; ++number)
    {
        expected.push_back(Alone(Line('a', number)));
    }
    expected.push_back(Alone(Line('b', 1)));
    for (int number = kMaxWaiting + 2; number <= 2 * (kMaxWaiting + 1); ++number)
    {
        expected.push_back(Alone(Line('a', number)));
    }
    EXPECT_EQ(m_socket.Sent(), expected);
}

// A thread is at its limit: the sender thread is stuck sending its first buffer, eight more are waiting, and the
// tenth is full but still with the thread. Once the sender thread can send again, it is the sender thread that
// sends the waiting buffers, oldest first.
TEST_F(BufferedWriterTest, SenderThreadDrainsAThreadThatIsAtItsLimit)
{
    StartWriter(kOneLine);
    RecordingThread& thread = NewThread();
    ASSERT_NO_FATAL_FAILURE(FillUp(thread, 'a'));
    EXPECT_TRUE(m_socket.Sent().empty());

    // The buffer that was stuck and the eight that were waiting are sent. The tenth is not: the thread has not
    // handed it over, no timed flush is due, and the writer is still on.
    m_socket.ReleaseWrites();
    ASSERT_TRUE(m_socket.WaitForDatagrams(kMaxWaiting + 1));
    EXPECT_EQ(m_socket.Sent(), EachAlone('a', 1, kMaxWaiting + 1));

    // The thread's next line makes it hand the tenth buffer over, and fills a buffer itself. Both are sent.
    ASSERT_TRUE(thread.Record(Line('a', kMaxWaiting + 3)));
    ASSERT_TRUE(m_socket.WaitForDatagrams(kMaxWaiting + 3));
    EXPECT_EQ(m_socket.Sent(), EachAlone('a', 1, kMaxWaiting + 3));

    // All of them were sent by one thread, which is neither the thread that recorded them nor this one, which has
    // not turned the writer off yet: the sender thread.
    const std::vector<FakeSocketWriter::Datagram> datagrams = m_socket.SentDatagrams();
    const std::thread::id senderThread = datagrams.front().sentBy;
    EXPECT_NE(senderThread, thread.Id());
    EXPECT_NE(senderThread, std::this_thread::get_id());
    for (const FakeSocketWriter::Datagram& datagram : datagrams)
    {
        EXPECT_EQ(datagram.sentBy, senderThread);
    }
}

TEST_F(BufferedWriterTest, SenderKeepsGoingAfterAFailedSend)
{
    StartWriter(kOneLine);
    m_socket.FailNextWrite();

    EXPECT_TRUE(buffered_writer::Record(Line('a', 1)));  // the send of this one throws
    EXPECT_TRUE(buffered_writer::Record(Line('a', 2)));

    ASSERT_TRUE(m_socket.WaitForDatagrams(1));
    EXPECT_EQ(m_socket.FailedWrites(), 1u);
    EXPECT_EQ(m_socket.Sent(), (Datagrams{Alone(Line('a', 2))}));
}

TEST_F(BufferedWriterTest, ThreadGetsNewBuffersAfterARestart)
{
    RecordingThread& thread = NewThread();

    StartWriter(4096);
    EXPECT_TRUE(buffered_writer::Record("this.thread.first"));
    EXPECT_TRUE(thread.Record("another.thread.first"));
    buffered_writer::Stop();
    EXPECT_EQ(m_socket.Sent(), (Datagrams{"this.thread.first\n", "another.thread.first\n"}));

    // Both threads still have the buffers of the first writer. What they record now goes to the second one.
    buffered_writer::Start(&m_secondSocket, 4096, kNoTimedFlush, FullBufferPolicy::Block);
    EXPECT_TRUE(buffered_writer::Record("this.thread.second"));
    EXPECT_TRUE(thread.Record("another.thread.second"));
    buffered_writer::Stop();
    EXPECT_EQ(m_secondSocket.Sent(), (Datagrams{"this.thread.second\n", "another.thread.second\n"}));
    EXPECT_EQ(m_socket.Sent().size(), 2u);
}

// What the Writer does when the process exits.
TEST_F(BufferedWriterTest, ShutdownAtExitSendsWhatIsBufferedAndDropsLaterLines)
{
    StartWriter(4096);
    RecordingThread& thread = NewThread();

    EXPECT_TRUE(buffered_writer::Record("this.thread.before"));
    ASSERT_TRUE(thread.Record("another.thread.before"));
    buffered_writer::ShutdownAtExit();
    EXPECT_EQ(m_socket.Sent(), (Datagrams{"this.thread.before\n", "another.thread.before\n"}));

    // Buffering stays on, so the lines are taken here, and not handed back to a caller that would send them through
    // a socket writer that is being destroyed. They are dropped.
    RecordingThread& newThread = NewThread();
    EXPECT_TRUE(buffered_writer::Record("this.thread.after"));
    EXPECT_TRUE(thread.Record("another.thread.after"));
    EXPECT_TRUE(newThread.Record("new.thread.after"));

    // A thread can still exit, which lets go of buffers that nobody is going to send any more.
    EXPECT_TRUE(thread.Exit());
    buffered_writer::Stop();
    EXPECT_EQ(m_socket.Sent().size(), 2u);
}
