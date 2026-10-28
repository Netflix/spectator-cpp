#include <writer_test_helper.h>

#include <recording_writer.h>
#include <wait_for.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace spectator;
using namespace std::chrono_literals;
using spectator::detail::ThreadBuffer;
using spectator::test::Recording;
using spectator::test::RecordingWriter;
using spectator::test::WaitFor;
using Payloads = std::vector<std::string>;

// A standalone Writer over a transport, with the buffer operations under short names.
class TestWriter
{
   public:
    TestWriter(std::unique_ptr<BaseWriter> transport, const BufferConfig& config)
        : m_writer(WriterTestHelper::MakeWriter(std::move(transport), config))
    {
    }

    bool IsBuffered() const { return WriterTestHelper::IsBuffered(*m_writer); }
    std::shared_ptr<ThreadBuffer> RegisterBuffer() { return WriterTestHelper::RegisterBuffer(*m_writer); }
    void Append(ThreadBuffer& buffer, std::string_view line) { WriterTestHelper::Append(*m_writer, buffer, line); }
    void RetireBuffer(ThreadBuffer& buffer) { WriterTestHelper::RetireBuffer(*m_writer, buffer); }
    void WriteLine(std::string_view line, std::string& scratch) { WriterTestHelper::WriteLine(*m_writer, line, scratch); }
    void RunDrainPass() { WriterTestHelper::RunDrainPass(*m_writer); }
    void Shutdown() { WriterTestHelper::Shutdown(*m_writer); }
    std::uint64_t DroppedCount() const { return WriterTestHelper::DroppedCount(*m_writer); }
    std::size_t BufferCount() const { return WriterTestHelper::BufferCount(*m_writer); }

   private:
    std::unique_ptr<Writer> m_writer;
};

class WriterBuffersTest : public testing::Test
{
   protected:
    // A writer over a recording transport. The default flush interval is long, so only the tests that want the drain
    // thread to run set a short one.
    std::unique_ptr<TestWriter> MakeState(unsigned int size, bool dropOnFull = false,
                                           std::chrono::milliseconds flushInterval = 1h)
    {
        return std::make_unique<TestWriter>(std::make_unique<RecordingWriter>(recording),
                                             BufferConfig{.size = size, .flushInterval = flushInterval,
                                                          .dropOnFull = dropOnFull});
    }

    std::shared_ptr<Recording> recording = std::make_shared<Recording>();
};

// ---- Unbuffered ----

TEST_F(WriterBuffersTest, UnbufferedSendsEachLineAsItsOwnDatagram)
{
    auto state = MakeState(0);
    EXPECT_FALSE(state->IsBuffered());
    EXPECT_EQ(state->RegisterBuffer(), nullptr);

    std::string scratch;
    state->WriteLine("c:a:1", scratch);
    state->WriteLine("c:b:2", scratch);
    EXPECT_EQ(recording->Payloads(), (Payloads{"c:a:1\n", "c:b:2\n"}));
}

TEST_F(WriterBuffersTest, UnbufferedWriteAfterCloseIsDropped)
{
    auto state = MakeState(0);
    state->Shutdown();

    std::string scratch;
    state->WriteLine("c:a:1", scratch);
    EXPECT_EQ(recording->Count(), 0u);
}

// ---- Buffer behavior ----

TEST_F(WriterBuffersTest, HoldsLinesUntilDrained)
{
    auto state = MakeState(64);
    auto buffer = state->RegisterBuffer();
    ASSERT_NE(buffer, nullptr);
    state->Append(*buffer, "c:a:1");
    state->Append(*buffer, "c:b:2");
    EXPECT_EQ(recording->Count(), 0u);

    state->RunDrainPass();
    EXPECT_EQ(recording->Payloads(), (Payloads{"c:a:1\nc:b:2\n"}));

    // Nothing is pending, so nothing more is sent.
    state->RunDrainPass();
    EXPECT_EQ(recording->Count(), 1u);
}

TEST_F(WriterBuffersTest, ExactFitStaysBufferedAndOneMoreByteFlushes)
{
    auto state = MakeState(8);
    auto buffer = state->RegisterBuffer();
    state->Append(*buffer, "abc");
    state->Append(*buffer, "def");
    EXPECT_EQ(recording->Count(), 0u) << "8 bytes fit a capacity of 8 exactly";

    // The next line doesn't fit, so this thread sends the full buffer itself and keeps the new line.
    state->Append(*buffer, "g");
    const auto datagrams = recording->Datagrams();
    ASSERT_EQ(datagrams.size(), 1u);
    EXPECT_EQ(datagrams[0].payload, "abc\ndef\n");
    EXPECT_EQ(datagrams[0].sender, std::this_thread::get_id());

    state->RetireBuffer(*buffer);
    EXPECT_EQ(recording->Payloads(), (Payloads{"abc\ndef\n", "g\n"}));
}

TEST_F(WriterBuffersTest, DropOnFullDropsTheLineAndWakesTheDrainThread)
{
    auto state = MakeState(16, /*dropOnFull=*/true);  // a one-hour interval, so only the early wake can drain
    auto buffer = state->RegisterBuffer();
    state->Append(*buffer, "0123456");
    state->Append(*buffer, "0123456");
    state->Append(*buffer, "x");
    EXPECT_EQ(state->DroppedCount(), 1u);

    ASSERT_TRUE(WaitFor([&] { return recording->Count() == 1; }, 2s));
    const auto datagrams = recording->Datagrams();
    EXPECT_EQ(datagrams[0].payload, "0123456\n0123456\n") << "the dropped line is gone, the buffered ones are kept";
    EXPECT_NE(datagrams[0].sender, std::this_thread::get_id()) << "in drop mode the writing thread never sends";

    // The buffer has room again.
    state->Append(*buffer, "y");
    EXPECT_EQ(state->DroppedCount(), 1u);
}

TEST_F(WriterBuffersTest, OversizedLineIsSentAloneAfterPendingLinesInBothModes)
{
    for (const bool dropOnFull : {false, true})
    {
        SCOPED_TRACE(dropOnFull ? "drop on full" : "flush on full");
        auto testRecording = std::make_shared<Recording>();
        TestWriter state(std::make_unique<RecordingWriter>(testRecording),
                          BufferConfig{.size = 8, .flushInterval = 1h, .dropOnFull = dropOnFull});
        auto buffer = state.RegisterBuffer();

        state.Append(*buffer, "ab");
        const std::string oversized(20, 'x');
        state.Append(*buffer, oversized);
        EXPECT_EQ(testRecording->Payloads(), (Payloads{"ab\n", oversized + "\n"}));
        EXPECT_EQ(state.DroppedCount(), 0u) << "an oversized line is never dropped";

        // The buffer is empty again and keeps buffering.
        state.Append(*buffer, "c");
        EXPECT_EQ(testRecording->Count(), 2u);
    }
}

TEST_F(WriterBuffersTest, RetireBufferSendsPendingLinesAndLaterAppendsAreDropped)
{
    auto state = MakeState(64);
    auto buffer = state->RegisterBuffer();
    state->Append(*buffer, "c:a:1");

    state->RetireBuffer(*buffer);
    EXPECT_EQ(recording->Payloads(), (Payloads{"c:a:1\n"}));
    state->Append(*buffer, "c:b:1");

    // Appending to, closing again and draining a closed buffer send nothing.
    state->RetireBuffer(*buffer);
    state->RunDrainPass();
    EXPECT_EQ(recording->Count(), 1u);
}

TEST_F(WriterBuffersTest, RetireBufferWithNothingPendingSendsNothing)
{
    auto state = MakeState(64);
    auto buffer = state->RegisterBuffer();
    state->RetireBuffer(*buffer);
    EXPECT_EQ(recording->Count(), 0u);
}

TEST_F(WriterBuffersTest, LinesStayInOrderWhileTheDrainRacesTheWriter)
{
    // A small capacity makes the writing thread flush often, while another thread drains in a tight loop. Every line
    // must still arrive exactly once and in the order it was written.
    constexpr unsigned int kCapacity = 32;
    constexpr int kLines = 20000;
    auto state = MakeState(kCapacity);
    auto buffer = state->RegisterBuffer();

    std::atomic<bool> stop{false};
    std::thread drainer(
        [&]
        {
            while (!stop.load())
            {
                state->RunDrainPass();
            }
        });
    for (int i = 0; i < kLines; ++i)
    {
        state->Append(*buffer, std::to_string(i));
    }
    stop = true;
    drainer.join();
    state->RetireBuffer(*buffer);

    const auto lines = recording->Lines();
    ASSERT_EQ(lines.size(), static_cast<std::size_t>(kLines));
    for (int i = 0; i < kLines; ++i)
    {
        ASSERT_EQ(lines[i], std::to_string(i)) << "line " << i << " arrived out of order";
    }
    for (const auto& payload : recording->Payloads())
    {
        EXPECT_LE(payload.size(), kCapacity);
    }
}

TEST_F(WriterBuffersTest, BufferSizeIsClampedToTheTransportLimit)
{
    TestWriter state(std::make_unique<RecordingWriter>(recording, 0, 100),
                      BufferConfig{.size = 1000, .flushInterval = 1h});
    auto buffer = state.RegisterBuffer();
    for (int i = 0; i < 50; ++i)
    {
        state.Append(*buffer, "c:clamp:1.000000");
    }
    state.Shutdown();

    EXPECT_EQ(recording->Lines().size(), 50u);
    EXPECT_GT(recording->Count(), 1u);
    for (const auto& payload : recording->Payloads())
    {
        EXPECT_LE(payload.size(), 100u);
    }
}

// ---- Drain thread and roster ----

TEST_F(WriterBuffersTest, IntervalDrainSendsPartialBuffers)
{
    auto state = MakeState(1024, false, 50ms);
    auto buffer = state->RegisterBuffer();
    state->Append(*buffer, "c:a:1");

    ASSERT_TRUE(WaitFor([&] { return recording->Count() == 1; }));
    const auto datagrams = recording->Datagrams();
    EXPECT_EQ(datagrams[0].payload, "c:a:1\n");
    EXPECT_NE(datagrams[0].sender, std::this_thread::get_id()) << "the drain thread sends partial buffers";
}

TEST_F(WriterBuffersTest, BuffersOfExitedThreadsAreSentAtOnceAndReaped)
{
    auto state = MakeState(1024, false, 20ms);
    std::thread worker(
        [&]
        {
            auto buffer = state->RegisterBuffer();
            state->Append(*buffer, "c:from.worker:1");
            // What a thread's handle does when the thread exits.
            state->RetireBuffer(*buffer);
            EXPECT_EQ(recording->Payloads(), (Payloads{"c:from.worker:1\n"})) << "sent at exit, not at the next pass";
        });
    worker.join();

    ASSERT_TRUE(WaitFor([&] { return state->BufferCount() == 0; })) << "the drain thread forgets closed buffers";
    EXPECT_EQ(recording->Count(), 1u);
}

TEST_F(WriterBuffersTest, RunDrainPassSendsAndReaps)
{
    auto state = MakeState(1024);
    auto live = state->RegisterBuffer();
    auto exited = state->RegisterBuffer();
    state->Append(*live, "c:live:1");
    state->Append(*exited, "c:exited:1");
    state->RetireBuffer(*exited);
    EXPECT_EQ(state->BufferCount(), 2u);

    state->RunDrainPass();
    EXPECT_EQ(recording->Payloads(), (Payloads{"c:exited:1\n", "c:live:1\n"}));
    EXPECT_EQ(state->BufferCount(), 1u);
}

// ---- Shutdown ----

TEST_F(WriterBuffersTest, ShutdownSendsEverythingClosesEveryBufferAndTheTransport)
{
    auto state = MakeState(1024);
    auto a = state->RegisterBuffer();
    auto b = state->RegisterBuffer();
    state->Append(*a, "c:a:1");
    state->Append(*b, "c:b:1");

    const auto start = std::chrono::steady_clock::now();
    state->Shutdown();
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s) << "Shutdown must not wait for the interval";

    auto lines = recording->Lines();
    std::sort(lines.begin(), lines.end());
    EXPECT_EQ(lines, (std::vector<std::string>{"c:a:1", "c:b:1"}));
    state->Append(*a, "c:a:2");
    EXPECT_EQ(state->RegisterBuffer(), nullptr);
    std::string scratch;
    state->WriteLine("c:late:1", scratch);

    state->Shutdown();
    EXPECT_EQ(recording->Count(), 2u) << "a second Shutdown, and writes after Shutdown, send nothing more";
}

TEST_F(WriterBuffersTest, ShutdownRightAfterStartDoesNotWaitForTheInterval)
{
    // Closing while the drain thread is between checking for a stop and starting its wait is the lost-wakeup race.
    // Repeating it makes hitting that window likely; a lost wakeup would block for an hour.
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 200; ++i)
    {
        auto state = MakeState(64);
        state->Shutdown();
    }
    EXPECT_LT(std::chrono::steady_clock::now() - start, 10s);
}

TEST_F(WriterBuffersTest, ManyThreadsKeepTheirOrderAndLoseNothing)
{
    constexpr unsigned int kCapacity = 256;
    constexpr int kThreads = 8;
    constexpr int kLinesPerThread = 5000;
    auto state = MakeState(kCapacity, false, 1ms);

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back(
            [&state, t]
            {
                auto buffer = state->RegisterBuffer();
                for (int i = 0; i < kLinesPerThread; ++i)
                {
                    state->Append(*buffer, "t" + std::to_string(t) + ":" + std::to_string(i));
                }
                state->RetireBuffer(*buffer);
            });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }
    state->Shutdown();

    std::map<int, int> nextSequence;
    std::size_t total = 0;
    for (const auto& line : recording->Lines())
    {
        const auto colon = line.find(':');
        ASSERT_NE(colon, std::string::npos) << line;
        const int thread = std::stoi(line.substr(1, colon - 1));
        const int sequence = std::stoi(line.substr(colon + 1));
        ASSERT_EQ(sequence, nextSequence[thread]) << "thread " << thread << " lost or reordered a line";
        ++nextSequence[thread];
        ++total;
    }
    EXPECT_EQ(total, static_cast<std::size_t>(kThreads * kLinesPerThread));
    for (const auto& payload : recording->Payloads())
    {
        EXPECT_LE(payload.size(), kCapacity);
    }
}

TEST_F(WriterBuffersTest, DropModeWritersNeverWaitForAStalledTransport)
{
    auto state = MakeState(64, /*dropOnFull=*/true, 1ms);
    auto buffer = state->RegisterBuffer();

    // Stall the transport, then let the drain thread take a line and get stuck sending it.
    recording->Block();
    struct Unblock
    {
        Recording& recording;
        ~Unblock() { recording.Unblock(); }
    } unblockOnExit{*recording};
    state->Append(*buffer, "c:first:1");
    ASSERT_TRUE(WaitFor([&] { return recording->BlockedWriters() == 1; }));

    // The writer must finish every append while the drain thread is stuck: lines are buffered or dropped, never sent
    // from this thread.
    std::atomic<bool> done{false};
    std::thread writer(
        [&]
        {
            for (int i = 0; i < 10000; ++i)
            {
                state->Append(*buffer, "c:line:" + std::to_string(i));
            }
            done = true;
        });
    EXPECT_TRUE(WaitFor([&] { return done.load(); }, 5s)) << "a drop-mode writer waited for the stalled transport";

    recording->Unblock();
    writer.join();
    EXPECT_GT(state->DroppedCount(), 0u);
    state->Shutdown();
}
