#include <memory_writer.h>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

using namespace spectator;

TEST(MemoryWriterTest, IsEmpty)
{
    const auto writer = MemoryWriter();
    EXPECT_TRUE(writer.IsEmpty());
}

TEST(MemoryWriterTest, Write)
{
    auto writer = MemoryWriter();
    writer.Write("Test message");
    EXPECT_FALSE(writer.IsEmpty());
    EXPECT_EQ(writer.LastLine(), "Test message");
}

TEST(MemoryWriterTest, Clear)
{
    auto writer = MemoryWriter();
    writer.Write("Test message");
    EXPECT_FALSE(writer.IsEmpty());

    writer.Clear();
    EXPECT_TRUE(writer.IsEmpty());
}

TEST(MemoryWriterTest, GetMessages)
{
    auto writer = MemoryWriter();
    writer.Write("First message");
    writer.Write("Second message");

    const auto& messages = writer.GetMessages();
    EXPECT_EQ(messages.size(), 2);
    EXPECT_EQ(messages[0], "First message");
    EXPECT_EQ(messages[1], "Second message");
}

TEST(MemoryWriterTest, LastLine)
{
    auto writer = MemoryWriter();
    writer.Write("First message");
    writer.Write("Second message");

    EXPECT_EQ(writer.LastLine(), "Second message");

    writer.Clear();
    EXPECT_EQ(writer.LastLine(), "");
}
TEST(MemoryWriterTest, ConcurrentWritesAreAllKept)
{
    MemoryWriter writer;
    constexpr int kThreads = 8;
    constexpr int kWritesPerThread = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back(
            [&writer, t]
            {
                for (int i = 0; i < kWritesPerThread; ++i)
                {
                    writer.Write("thread" + std::to_string(t));
                }
            });
    }
    // Reading while other threads write is safe.
    while (writer.GetMessages().size() < kThreads * kWritesPerThread / 2)
    {
        std::this_thread::yield();
    }
    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(writer.GetMessages().size(), static_cast<std::size_t>(kThreads * kWritesPerThread));
}
