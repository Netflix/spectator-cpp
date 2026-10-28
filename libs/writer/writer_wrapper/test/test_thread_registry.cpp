#include <thread_registry.h>

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
using spectator::detail::RegistryOptions;
using spectator::detail::Sink;
using spectator::detail::ThreadBuffer;
using spectator::detail::ThreadRegistry;
using spectator::test::Recording;
using spectator::test::RecordingWriter;
using spectator::test::WaitFor;
using Result = ThreadBuffer::AppendResult;
using Payloads = std::vector<std::string>;

class ThreadRegistryTest : public testing::Test
{
   protected:
    std::shared_ptr<Recording> recording = std::make_shared<Recording>();
    std::shared_ptr<Sink> sink = std::make_shared<Sink>(std::make_unique<RecordingWriter>(recording));
};

TEST_F(ThreadRegistryTest, IntervalDrainSendsPartialBuffers)
{
    ThreadRegistry registry(sink, RegistryOptions{.capacity = 1024, .dropOnFull = false, .flushInterval = 50ms});
    auto buffer = registry.CreateBuffer();
    ASSERT_NE(buffer, nullptr);
    ASSERT_EQ(buffer->Append("c:a:1"), Result::Appended);

    ASSERT_TRUE(WaitFor([&] { return recording->Count() == 1; }));
    const auto datagrams = recording->Datagrams();
    EXPECT_EQ(datagrams[0].payload, "c:a:1\n");
    EXPECT_NE(datagrams[0].sender, std::this_thread::get_id()) << "the drain thread sends partial buffers";
}

TEST_F(ThreadRegistryTest, DropWakesTheDrainThreadEarly)
{
    ThreadRegistry registry(sink, RegistryOptions{.capacity = 16, .dropOnFull = true, .flushInterval = 1h});
    auto buffer = registry.CreateBuffer();
    ASSERT_EQ(buffer->Append("0123456"), Result::Appended);
    ASSERT_EQ(buffer->Append("0123456"), Result::Appended);
    ASSERT_EQ(buffer->Append("x"), Result::Dropped);

    // Without the early wake this would wait an hour.
    ASSERT_TRUE(WaitFor([&] { return recording->Count() == 1; }, 2s));
    EXPECT_EQ(recording->Payloads(), (Payloads{"0123456\n0123456\n"}));
    EXPECT_EQ(registry.DroppedCount(), 1u);

    // The buffer has room again.
    EXPECT_EQ(buffer->Append("y"), Result::Appended);
}

TEST_F(ThreadRegistryTest, BuffersOfExitedThreadsAreSentAtOnceAndReaped)
{
    ThreadRegistry registry(sink, RegistryOptions{.capacity = 1024, .dropOnFull = false, .flushInterval = 20ms});
    std::thread worker(
        [&]
        {
            auto buffer = registry.CreateBuffer();
            ASSERT_EQ(buffer->Append("c:from.worker:1"), Result::Appended);
            // What the Writer does when a thread exits.
            buffer->FlushAndClose();
            EXPECT_EQ(recording->Payloads(), (Payloads{"c:from.worker:1\n"})) << "sent at exit, not at the next pass";
        });
    worker.join();

    ASSERT_TRUE(WaitFor([&] { return registry.BufferCount() == 0; })) << "the drain thread forgets closed buffers";
    EXPECT_EQ(recording->Count(), 1u);
}

TEST_F(ThreadRegistryTest, RunDrainPassSendsAndReaps)
{
    ThreadRegistry registry(sink, RegistryOptions{.capacity = 1024, .dropOnFull = false, .flushInterval = 1h});
    auto live = registry.CreateBuffer();
    auto exited = registry.CreateBuffer();
    ASSERT_EQ(live->Append("c:live:1"), Result::Appended);
    ASSERT_EQ(exited->Append("c:exited:1"), Result::Appended);
    exited->FlushAndClose();
    EXPECT_EQ(registry.BufferCount(), 2u);

    registry.RunDrainPass();
    EXPECT_EQ(recording->Payloads(), (Payloads{"c:exited:1\n", "c:live:1\n"}));
    EXPECT_EQ(registry.BufferCount(), 1u);
}

TEST_F(ThreadRegistryTest, CloseSendsEverythingAndClosesEveryBuffer)
{
    ThreadRegistry registry(sink, RegistryOptions{.capacity = 1024, .dropOnFull = false, .flushInterval = 1h});
    auto a = registry.CreateBuffer();
    auto b = registry.CreateBuffer();
    ASSERT_EQ(a->Append("c:a:1"), Result::Appended);
    ASSERT_EQ(b->Append("c:b:1"), Result::Appended);

    const auto start = std::chrono::steady_clock::now();
    registry.Close();
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s) << "Close must not wait for the interval";

    auto lines = recording->Lines();
    std::sort(lines.begin(), lines.end());
    EXPECT_EQ(lines, (std::vector<std::string>{"c:a:1", "c:b:1"}));
    EXPECT_EQ(a->Append("c:a:2"), Result::Closed);
    EXPECT_EQ(registry.CreateBuffer(), nullptr);

    registry.Close();
    EXPECT_EQ(recording->Count(), 2u);
}

TEST_F(ThreadRegistryTest, CloseRightAfterStartDoesNotWaitForTheInterval)
{
    // Closing while the drain thread is between checking for a stop and starting its wait is the lost-wakeup race.
    // Repeating it makes hitting that window likely; a lost wakeup would block for an hour.
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 200; ++i)
    {
        ThreadRegistry registry(sink, RegistryOptions{.capacity = 64, .dropOnFull = false, .flushInterval = 1h});
        registry.Close();
    }
    EXPECT_LT(std::chrono::steady_clock::now() - start, 10s);
}

TEST_F(ThreadRegistryTest, ManyThreadsKeepTheirOrderAndLoseNothing)
{
    constexpr std::size_t kCapacity = 256;
    constexpr int kThreads = 8;
    constexpr int kLinesPerThread = 5000;
    ThreadRegistry registry(sink, RegistryOptions{.capacity = kCapacity, .dropOnFull = false, .flushInterval = 1ms});

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back(
            [&registry, t]
            {
                auto buffer = registry.CreateBuffer();
                for (int i = 0; i < kLinesPerThread; ++i)
                {
                    ASSERT_EQ(buffer->Append("t" + std::to_string(t) + ":" + std::to_string(i)), Result::Appended);
                }
                buffer->FlushAndClose();
            });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }
    registry.Close();

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

TEST_F(ThreadRegistryTest, DropModeWritersNeverWaitForAStalledSink)
{
    ThreadRegistry registry(sink, RegistryOptions{.capacity = 64, .dropOnFull = true, .flushInterval = 1ms});
    auto buffer = registry.CreateBuffer();

    // Stall the transport, then let the drain thread take a line and get stuck sending it.
    recording->Block();
    struct Unblock
    {
        Recording& recording;
        ~Unblock() { recording.Unblock(); }
    } unblockOnExit{*recording};
    ASSERT_EQ(buffer->Append("c:first:1"), Result::Appended);
    ASSERT_TRUE(WaitFor([&] { return recording->BlockedWriters() == 1; }));

    // The writer must finish every append while the drain thread is stuck: lines are buffered or dropped, never sent
    // from this thread.
    std::atomic<bool> done{false};
    std::atomic<int> dropped{0};
    std::thread writer(
        [&]
        {
            for (int i = 0; i < 10000; ++i)
            {
                if (buffer->Append("c:line:" + std::to_string(i)) == Result::Dropped)
                {
                    ++dropped;
                }
            }
            done = true;
        });
    EXPECT_TRUE(WaitFor([&] { return done.load(); }, 5s)) << "a drop-mode writer waited for the stalled sink";

    recording->Unblock();
    writer.join();
    EXPECT_GT(dropped.load(), 0);
    EXPECT_EQ(registry.DroppedCount(), static_cast<std::uint64_t>(dropped.load()));
    registry.Close();
}
