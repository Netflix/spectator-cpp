#include <thread_local_buffered_write_mode.h>
#include <memory_writer.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace spectator;

// Counts total non-empty lines across all batches received by the MemoryWriter.
// Each GetMessages() entry is one flush - a newline-separated batch of messages.
static int CountLines(const MemoryWriter& writer)
{
    int count = 0;
    for (const auto& batch : writer.GetMessages())
    {
        std::istringstream ss(batch);
        std::string line;
        while (std::getline(ss, line))
        {
            if (!line.empty()) ++count;
        }
    }
    return count;
}

// A message that fills the buffer exactly on the first write should flush immediately.
// Buffer = 20 bytes; message "aaaaaaaaaaaaaaaaaaa" (19 chars) + '\n' = 20 bytes >= 20.
TEST(ThreadLocalBufferedWriteModeTest, SizeBasedFlushTriggered)
{
    ThreadLocalBufferedWriteMode mode(WriterType::Memory, 20, std::chrono::seconds(100));
    auto* writer = static_cast<MemoryWriter*>(mode.GetWriter());

    const std::string msg(19, 'a');
    std::thread t([&]() { mode.Write(msg); });
    t.join();

    EXPECT_FALSE(writer->IsEmpty());
}

// A message well below the buffer threshold should remain buffered until the thread exits.
TEST(ThreadLocalBufferedWriteModeTest, SmallWriteNotFlushedBeforeThreadExit)
{
    ThreadLocalBufferedWriteMode mode(WriterType::Memory, 1000, std::chrono::seconds(100));
    auto* writer = static_cast<MemoryWriter*>(mode.GetWriter());

    std::atomic<bool> writeComplete{false};
    std::atomic<bool> canExit{false};

    std::thread t([&]()
    {
        mode.Write("hello");
        writeComplete.store(true);
        while (!canExit.load()) {}
    });

    while (!writeComplete.load()) {}
    EXPECT_TRUE(writer->IsEmpty());  // still in thread-local buffer

    canExit.store(true);
    t.join();
    EXPECT_FALSE(writer->IsEmpty());  // flushed by ThreadLocalData destructor on thread exit
}

// Thread exit should flush any remaining data in the thread-local buffer.
TEST(ThreadLocalBufferedWriteModeTest, ThreadExitFlushesRemainingData)
{
    ThreadLocalBufferedWriteMode mode(WriterType::Memory, 1000, std::chrono::seconds(100));
    auto* writer = static_cast<MemoryWriter*>(mode.GetWriter());

    std::thread t([&]() { mode.Write("flushed_on_exit"); });
    t.join();

    ASSERT_EQ(writer->GetMessages().size(), 1u);
    EXPECT_NE(writer->GetMessages()[0].find("flushed_on_exit"), std::string::npos);
}

// Data written below the size threshold should be delivered by the background flush
// thread once the flush interval has elapsed.
TEST(ThreadLocalBufferedWriteModeTest, TimedFlushDeliversStaleData)
{
    // flushInterval = 1s; flush thread wakes every 1s, so worst-case delivery is ~2s
    ThreadLocalBufferedWriteMode mode(WriterType::Memory, 1000, std::chrono::seconds(1));
    auto* writer = static_cast<MemoryWriter*>(mode.GetWriter());

    std::atomic<bool> writeComplete{false};
    std::atomic<bool> canExit{false};

    std::thread t([&]()
    {
        mode.Write("stale_data");
        writeComplete.store(true);
        while (!canExit.load()) {}
    });

    while (!writeComplete.load()) {}
    EXPECT_TRUE(writer->IsEmpty());  // not yet flushed

    std::this_thread::sleep_for(std::chrono::seconds(3));
    EXPECT_FALSE(writer->IsEmpty());  // flushed by background flush thread

    canExit.store(true);
    t.join();
}

// Each thread has an independent buffer: all threads write below the size threshold,
// so each thread produces exactly one flush (on exit). Total batches == numThreads,
// total lines == numThreads * writesPerThread.
TEST(ThreadLocalBufferedWriteModeTest, MultipleThreadsGetIndependentBuffers)
{
    ThreadLocalBufferedWriteMode mode(WriterType::Memory, 1000, std::chrono::seconds(100));
    auto* writer = static_cast<MemoryWriter*>(mode.GetWriter());

    constexpr int numThreads = 4;
    constexpr int writesPerThread = 3;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&]()
        {
            for (int j = 0; j < writesPerThread; ++j)
            {
                mode.Write("msg");
            }
        });
    }
    for (auto& t : threads) { t.join(); }

    EXPECT_EQ(static_cast<int>(writer->GetMessages().size()), numThreads);
    EXPECT_EQ(CountLines(*writer), numThreads * writesPerThread);
}

// Under concurrent load from many threads, no lines should be lost or corrupted.
TEST(ThreadLocalBufferedWriteModeTest, ConcurrentWritesProduceAllLines)
{
    ThreadLocalBufferedWriteMode mode(WriterType::Memory, 1000, std::chrono::seconds(100));
    auto* writer = static_cast<MemoryWriter*>(mode.GetWriter());

    constexpr int numThreads = 8;
    constexpr int writesPerThread = 10;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&]()
        {
            for (int j = 0; j < writesPerThread; ++j)
            {
                mode.Write("data");
            }
        });
    }
    for (auto& t : threads) { t.join(); }

    EXPECT_EQ(CountLines(*writer), numThreads * writesPerThread);
}
